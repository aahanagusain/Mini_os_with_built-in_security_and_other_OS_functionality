#include "../include/fs.h"
#include "../include/tty.h"
#include "../include/memory.h"
#include "../include/string.h"

/*
 * Phase 3 (begin): In-memory hierarchical node tree for ramfs.
 * We're adding lightweight node structures and helpers to gradually
 * migrate from flat overlay string semantics to a tree. This file will
 * continue to support existing overlay APIs while providing utilities
 * for future refactors.
 */

struct ram_node {
    char *name; /* local name of this node (not full path), e.g. "etc" */
    struct ram_node *parent;
    struct ram_node *first_child;
    struct ram_node *next_sibling;
    uint8_t *data; /* owned copy for overlay-created files, NULL for packaged files */
    size_t size;
    int is_dir;
    unsigned int uid;
    unsigned int gid;
    unsigned int mode;
    const struct fs_file *packaged; /* pointer to packaged file if present */
};

/* Root of the in-memory tree (represents "/"). Lazily initialised. */
static struct ram_node *ram_root = NULL;

/* helpers using kernel allocator (kmalloc/kfree) */
static char *kstrdup(const char *s);
static void *krealloc(void *old, size_t oldsz, size_t newsz);

/* Forward declarations for functions used before their definitions. */
static void build_tree_from_initrd_if_needed(void);
static int path_to_components(const char *path, char components[][128], int max_comps);
static struct ram_node *find_child(struct ram_node *parent, const char *name);
static struct ram_node *insert_child(struct ram_node *parent, const char *name, int is_dir);

/* The packer will generate these symbols in src/initrd_data.c */
/* (declarations are already provided above) */

/* Simple dynamic ramfs overlay structures for Phase 2 */
#define OVERLAY_MAX_FILES 64

struct overlay_file {
    char *name;
    uint8_t *data;
    size_t size;
    int is_dir;
    int used;
    unsigned int uid;
    unsigned int gid;
    unsigned int mode; /* permission bits */
};

static struct overlay_file overlay[OVERLAY_MAX_FILES];

static int overlay_find(const char *path);
static int overlay_alloc(const char *path, const uint8_t *data, size_t size, int is_dir);
static void overlay_free(int idx);


static struct ram_node *node_create(const char *name, int is_dir)
{
    struct ram_node *n = kmalloc(sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    if (name) n->name = kstrdup(name);
    else n->name = kstrdup("/");
    n->is_dir = is_dir;
    n->uid = 0; n->gid = 0; n->mode = is_dir ? 0755 : 0644;
    return n;
}

static void node_free_recursive(struct ram_node *n)
{
    if (!n) return;
    struct ram_node *c = n->first_child;
    while (c) {
        struct ram_node *next = c->next_sibling;
        node_free_recursive(c);
        c = next;
    }
    if (n->name) kfree(n->name);
    if (n->data) kfree(n->data);
    kfree(n);
}

/* Remove node from its parent child list and free it recursively. */
static void remove_node(struct ram_node *n)
{
    if (!n || !n->parent) { node_free_recursive(n); return; }
    struct ram_node *p = n->parent;
    struct ram_node **slot = &p->first_child;
    while (*slot) {
        if (*slot == n) {
            *slot = n->next_sibling;
            node_free_recursive(n);
            return;
        }
        slot = &((*slot)->next_sibling);
    }
}

/* Detach node from its parent but don't free it. Caller must reattach or free later. */
static void detach_node(struct ram_node *n)
{
    if (!n || !n->parent) return;
    struct ram_node *p = n->parent;
    struct ram_node **slot = &p->first_child;
    while (*slot) {
        if (*slot == n) {
            *slot = n->next_sibling;
            n->next_sibling = NULL;
            n->parent = NULL;
            return;
        }
        slot = &((*slot)->next_sibling);
    }
}

/* Build full path of a node into a static buffer and return it. */
static const char *node_fullpath(const struct ram_node *n)
{
    static char buf[1024];
    if (!n) { buf[0] = '\0'; return buf; }
    /* collect components up to root */
    const struct ram_node *stack[64];
    int top = 0;
    const struct ram_node *cur = n;
    while (cur && cur->parent) {
        stack[top++] = cur;
        cur = cur->parent;
        if (top >= (int)(sizeof(stack)/sizeof(stack[0]))-1) break;
    }
    /* root path */
    if (top == 0) { strcpy(buf, "/"); return buf; }
    buf[0] = '\0';
    for (int i = top - 1; i >= 0; --i) {
        strcat(buf, "/");
        strcat(buf, stack[i]->name ? stack[i]->name : "");
    }
    return buf;
}

/* Insert overlay-backed node for given path and overlay index. Creates intermediate directories as needed. */
static int insert_overlay_node_for_index(const char *path, int overlay_idx, int is_dir)
{
    if (!path) return -1;
    build_tree_from_initrd_if_needed();
    char comps[32][128];
    int c = path_to_components(path, comps, 32);
    struct ram_node *cur = ram_root;
    if (!cur) return -1;
    for (int i = 0; i < c; ++i) {
        int last = (i == c-1);
        struct ram_node *n = find_child(cur, comps[i]);
        if (!n) {
            n = insert_child(cur, comps[i], last ? is_dir : 1);
            if (!n) return -1;
        }
        cur = n;
    }
    /* attach overlay data */
    cur->data = overlay[overlay_idx].data;
    cur->size = overlay[overlay_idx].size;
    cur->uid = overlay[overlay_idx].uid;
    cur->gid = overlay[overlay_idx].gid;
    cur->mode = overlay[overlay_idx].mode;
    cur->is_dir = overlay[overlay_idx].is_dir;
    cur->packaged = NULL;
    return 0;
}

/* split path into components starting after leading '/'. Returns count (0 for root).
 * components is an array of char[128] entries. max_comps must be >0. */
/* Forward declarations for functions used before their definitions below. */
static void build_tree_from_initrd_if_needed(void);
static int path_to_components(const char *path, char components[][128], int max_comps);
static struct ram_node *find_child(struct ram_node *parent, const char *name);
static struct ram_node *insert_child(struct ram_node *parent, const char *name, int is_dir);

/* The packer will generate these symbols in src/initrd_data.c */
extern const struct fs_file initrd_files[];
extern const unsigned int initrd_files_count;

static int path_to_components(const char *path, char components[][128], int max_comps)
{
    if (!path || path[0] != '/') return -1;
    const char *seg = path + 1; /* skip leading slash */
    int count = 0;
    while (*seg != '\0' && count < max_comps) {
        const char *slash = strchr(seg, '/');
        size_t len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (len == 0) { seg = slash ? (slash + 1) : seg + len; continue; }
        size_t copylen = len < 127 ? len : 127;
        memcpy(components[count], seg, copylen);
        components[count][copylen] = '\0';
        count++;
        if (!slash) break;
        seg = slash + 1;
    }
    return count;
}

/* Find a child with given name under parent (non-recursive). */
static struct ram_node *find_child(struct ram_node *parent, const char *name)
{
    if (!parent) return NULL;
    struct ram_node *c = parent->first_child;
    while (c) {
        if (c->name && strcmp(c->name, name) == 0) return c;
        c = c->next_sibling;
    }
    return NULL;
}

/* Insert a node as a child of parent. Returns the new node or existing one. */
static struct ram_node *insert_child(struct ram_node *parent, const char *name, int is_dir)
{
    struct ram_node *exist = find_child(parent, name);
    if (exist) return exist;
    struct ram_node *n = node_create(name, is_dir);
    if (!n) return NULL;
    n->next_sibling = parent->first_child;
    parent->first_child = n;
    n->parent = parent;
    return n;
}

/* Find node by absolute path in the in-memory tree. Returns NULL if not found. */
static struct ram_node *find_node_by_path(const char *path)
{
    if (!path) return NULL;
    if (strcmp(path, "/") == 0) return ram_root;
    char comps[32][128];
    int c = path_to_components(path, comps, 32);
    if (c <= 0) return ram_root;
    struct ram_node *cur = ram_root;
    for (int i = 0; i < c && cur; ++i) {
        cur = find_child(cur, comps[i]);
    }
    return cur;
}

/* Lazily build a minimal tree from packaged initrd_entries so tools can inspect it.
 * This is conservative: it only adds nodes representing packaged paths and does
 * not remove or modify overlay array behaviour. It is safe to call multiple times. */
static void build_tree_from_initrd_if_needed(void)
{
    if (ram_root) return;
    ram_root = node_create("/", 1);
    if (!ram_root) return;
    for (unsigned int i = 0; i < initrd_files_count; ++i) {
        const struct fs_file *f = &initrd_files[i];
        if (!f || !f->name) continue;
        /* ignore empty names */
        if (strcmp(f->name, "/") == 0) continue;
        char comps[32][128];
        int c = path_to_components(f->name, comps, 32);
        struct ram_node *cur = ram_root;
        for (int j = 0; j < c; ++j) {
            int is_last = (j == c-1);
            struct ram_node *n = insert_child(cur, comps[j], is_last ? (f->data == NULL ? 1 : 0) : 1);
            if (!n) break;
            cur = n;
        }
        if (cur) cur->packaged = f;
    }
}

/* helpers using kernel allocator (kmalloc/kfree) */
static char *kstrdup(const char *s)
{
    if (!s) return NULL;
    size_t l = strlen(s) + 1;
    char *d = kmalloc(l);
    if (!d) return NULL;
    memcpy(d, (void *)s, l);
    return d;
}

static void *krealloc(void *old, size_t oldsz, size_t newsz)
{
    void *n = kmalloc(newsz);
    if (!n) return NULL;
    if (old && oldsz) memcpy(n, old, oldsz < newsz ? oldsz : newsz);
    if (old) kfree(old);
    return n;
}

/* Overlay helper implementations (defined once). */
static int overlay_find(const char *path)
{
    if (!path) return -1;
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (overlay[i].used && overlay[i].name && strcmp(overlay[i].name, path) == 0) return i;
    }
    return -1;
}

static int overlay_alloc(const char *path, const uint8_t *data, size_t size, int is_dir)
{
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (!overlay[i].used) {
            overlay[i].used = 1;
            overlay[i].name = kstrdup(path);
            if (!overlay[i].name) { overlay[i].used = 0; return -1; }
            if (size) {
                overlay[i].data = kmalloc(size);
                if (!overlay[i].data) { kfree(overlay[i].name); overlay[i].used = 0; return -1; }
                memcpy(overlay[i].data, (void *)data, size);
                overlay[i].size = size;
            } else {
                overlay[i].data = NULL;
                overlay[i].size = 0;
            }
            overlay[i].is_dir = is_dir;
            /* default ownership and permissions */
            overlay[i].uid = 0;
            overlay[i].gid = 0;
            overlay[i].mode = is_dir ? 0755 : 0644;
            return i;
        }
    }
    return -1;
}

static void overlay_free(int idx)
{
    if (idx < 0 || idx >= OVERLAY_MAX_FILES) return;
    if (!overlay[idx].used) return;
    if (overlay[idx].name) kfree(overlay[idx].name);
    if (overlay[idx].data) kfree(overlay[idx].data);
    overlay[idx].used = 0;
}


/* The packer will generate these symbols in src/initrd_data.c */
extern const struct fs_file initrd_files[];
extern const unsigned int initrd_files_count;

/* Simple fd table */
#define MAX_FDS 16
struct open_file {
    const struct fs_file *f;
    size_t pos;
    int flags;
    int used;
};

static struct open_file fd_table[MAX_FDS];

int fs_mount_initrd_embedded(void)
{
    /* clear fd table */
    for (int i = 0; i < MAX_FDS; ++i) fd_table[i].used = 0;
    /* clear overlay */
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) overlay[i].used = 0;
    /* rebuild tree helper state if needed */
    if (ram_root) { node_free_recursive(ram_root); ram_root = NULL; }
    build_tree_from_initrd_if_needed();
    /* sanity check: at least zero files ok */
    return FS_OK;
}

static const struct fs_file *find_file(const char *path)
{
    /* overlay entries take precedence */
    int oi = overlay_find(path);
    if (oi >= 0) {
        /* construct a temporary fs_file mapping to overlay data */
        static struct fs_file temp;
        temp.name = overlay[oi].name;
        temp.data = overlay[oi].data;
        temp.size = overlay[oi].size;
        temp.uid = overlay[oi].uid;
        temp.gid = overlay[oi].gid;
        temp.mode = overlay[oi].mode;
        return &temp;
    }

    /* Next, consult the in-memory node tree (built from packaged entries
     * and updated when overlay entries are created). This lets us return
     * either overlay-backed nodes or packaged nodes represented in the tree. */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n) {
        static struct fs_file temp;
        /* prefer node-owned data if present */
        if (n->data) {
            temp.name = node_fullpath(n); /* returns owned buffer? we'll implement stack-safe version below */
            /* node_fullpath will write into a static buffer */
            temp.data = n->data;
            temp.size = n->size;
            temp.uid = n->uid;
            temp.gid = n->gid;
            temp.mode = n->mode;
            return &temp;
        }
        if (n->packaged) return n->packaged;
    }

    /* fallback: scan packaged initrd entries (compat) */
    for (unsigned int i = 0; i < initrd_files_count; ++i) {
        if (initrd_files[i].name && strcmp(initrd_files[i].name, path) == 0) return &initrd_files[i];
    }
    return NULL;
}

fs_fd_t fs_open(const char *path, int flags)
{
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    for (int i = 0; i < MAX_FDS; ++i) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].f = f;
            fd_table[i].pos = 0;
            fd_table[i].flags = flags;
            return i;
        }
    }
    return FS_EMFILE; /* no descriptors */
}

/* Phase 2: create a file in the overlay. Overwrites if exists. */
int fs_create(const char *path, const uint8_t *data, size_t size)
{
    int oi = overlay_find(path);
    if (oi >= 0) {
        /* replace existing */
        kfree(overlay[oi].data);
        overlay[oi].data = NULL;
        if (size) {
            overlay[oi].data = kmalloc(size);
            if (!overlay[oi].data) return FS_EIO;
            memcpy(overlay[oi].data, (void *)data, size);
            overlay[oi].size = size;
        } else {
            overlay[oi].size = 0;
        }
        return FS_OK;
    }
    int idx = overlay_alloc(path, data, size, 0);
    if (idx >= 0) {
        /* insert node in tree for this overlay-backed file */
        insert_overlay_node_for_index(path, idx, 0);
        return FS_OK;
    }
    return FS_EMFILE;
}

/* write to an open file descriptor (append). */
int fs_write(fs_fd_t fd, const void *buf, size_t count)
{
    if (fd < 0 || fd >= MAX_FDS) return FS_EINVAL;
    if (!fd_table[fd].used) return FS_EINVAL;
    const struct fs_file *f = fd_table[fd].f;
    /* only overlay files are writable */
    int oi = overlay_find(f->name);
    if (oi < 0) return FS_EIO;
    size_t newsize = overlay[oi].size + count;
    uint8_t *ndata = krealloc(overlay[oi].data, overlay[oi].size, newsize);
    if (!ndata) return FS_EIO;
    memcpy(ndata + overlay[oi].size, (void *)buf, count);
    overlay[oi].data = ndata;
    overlay[oi].size = newsize;
    fd_table[fd].pos = overlay[oi].size; /* move pos to end */
    return (int)count;
}

int fs_unlink(const char *path)
{
    int oi = overlay_find(path);
    if (oi < 0) return FS_ENOENT;
    overlay_free(oi);
    /* update tree: if a node exists for this path, remove overlay backing */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n) {
        /* if packaged exists, restore packaged backing; otherwise remove node */
        if (n->packaged) {
            n->data = NULL;
            n->size = n->packaged->size;
        } else {
            remove_node(n);
        }
    }
    return FS_OK;
}

int fs_mkdir(const char *path)
{
    /* very small behaviour: create an overlay directory entry */
    int oi = overlay_find(path);
    if (oi >= 0) return FS_OK; /* already exists */
    int idx = overlay_alloc(path, NULL, 0, 1);
    if (idx >= 0) {
        insert_overlay_node_for_index(path, idx, 1);
        return FS_OK;
    }
    return FS_EMFILE;
}

int fs_read(fs_fd_t fd, void *buf, size_t count)
{
    if (fd < 0 || fd >= MAX_FDS) return FS_EINVAL;
    if (!fd_table[fd].used) return FS_EINVAL;
    const struct fs_file *f = fd_table[fd].f;
    size_t remain = f->size - fd_table[fd].pos;
    size_t need = (count < remain) ? count : remain;
    if (need == 0) return 0;
    memcpy(buf, f->data + fd_table[fd].pos, need);
    fd_table[fd].pos += need;
    return (int)need;
}

int fs_close(fs_fd_t fd)
{
    if (fd < 0 || fd >= MAX_FDS) return FS_EINVAL;
    if (!fd_table[fd].used) return FS_EINVAL;
    fd_table[fd].used = 0;
    return FS_OK;
}

int fs_stat(const char *path, struct fs_stat *st)
{
    /* check overlay first for metadata */
    int oi = overlay_find(path);
    if (oi >= 0) {
        if (st) {
            st->size = overlay[oi].size;
            st->is_dir = overlay[oi].is_dir;
            st->uid = overlay[oi].uid;
            st->gid = overlay[oi].gid;
            st->mode = overlay[oi].mode;
        }
        return FS_OK;
    }
    /* consult node tree (which includes packaged entries) for accurate dir/file info */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n) {
        if (st) {
            st->size = n->data ? n->size : (n->packaged ? n->packaged->size : 0);
            st->is_dir = n->is_dir ? 1 : 0;
            st->uid = n->uid; st->gid = n->gid; st->mode = n->mode;
        }
        return FS_OK;
    }
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    if (st) {
        st->size = f->size;
        st->is_dir = 0;
        st->uid = 0; st->gid = 0; st->mode = 0644; /* defaults for packaged files */
    }
    return FS_OK;
}

int fs_chmod(const char *path, unsigned int mode)
{
    /* prefer overlay entry */
    int oi = overlay_find(path);
    if (oi >= 0) {
        overlay[oi].mode = mode;
        /* mirror into node tree */
        build_tree_from_initrd_if_needed();
        insert_overlay_node_for_index(path, oi, 0);
        return FS_OK;
    }
    /* try node tree (packaged or overlay-mirrored) */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n) {
        n->mode = mode;
        return FS_OK;
    }
    /* packaged file: create overlay copy to store metadata */
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    int idx = overlay_alloc(path, f->data, f->size, 0);
    if (idx < 0) return FS_EIO;
    overlay[idx].mode = mode;
    insert_overlay_node_for_index(path, idx, 0);
    return FS_OK;
}

int fs_chown(const char *path, unsigned int uid, unsigned int gid)
{
    int oi = overlay_find(path);
    if (oi >= 0) {
        overlay[oi].uid = uid;
        overlay[oi].gid = gid;
        build_tree_from_initrd_if_needed();
        insert_overlay_node_for_index(path, oi, 0);
        return FS_OK;
    }
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n) {
        n->uid = uid; n->gid = gid; return FS_OK;
    }
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    int idx = overlay_alloc(path, f->data, f->size, 0);
    if (idx < 0) return FS_EIO;
    overlay[idx].uid = uid;
    overlay[idx].gid = gid;
    insert_overlay_node_for_index(path, idx, 0);
    return FS_OK;
}

int fs_readdir(unsigned int index, const struct fs_file **out)
{
    /* first return packaged initrd entries */
    if (index < initrd_files_count) {
        if (out) *out = &initrd_files[index];
        return FS_OK;
    }
    /* then overlay entries */
    unsigned int idx = index - initrd_files_count;
    unsigned int found = 0;
    static struct fs_file temp;
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (!overlay[i].used) continue;
        if (found == idx) {
            if (out) {
                temp.name = overlay[i].name;
                temp.data = overlay[i].data;
                temp.size = overlay[i].size;
                *out = &temp;
            }
            return FS_OK;
        }
        found++;
    }
    return FS_ENOENT;
}

/* Phase 3: list directory entries under `path`. Index enumerates entries.
 * We implement this by scanning overlay for names that start with path + '/'
 * and returning the Nth matching entry (non-recursive single path component).
 */
int fs_listdir(const char *path, unsigned int index, const struct fs_file **out)
{
    /* New: use in-memory node tree for directory listing */
    build_tree_from_initrd_if_needed();
    struct ram_node *d = find_node_by_path(path);
    if (!d) return FS_ENOENT;
    if (!d->is_dir) return FS_ENOENT;
    static struct fs_file temp;
    unsigned int found = 0;
    struct ram_node *c = d->first_child;
    while (c) {
        if (found == index) {
            temp.name = (char *)node_fullpath(c);
            if (c->data) {
                temp.data = c->data;
                temp.size = c->size;
            } else if (c->packaged) {
                temp.data = c->packaged->data;
                temp.size = c->packaged->size;
            } else {
                /* directory or placeholder */
                temp.data = NULL;
                temp.size = 0;
            }
            if (out) *out = &temp;
            return FS_OK;
        }
        found++;
        c = c->next_sibling;
    }
    return FS_ENOENT;
}

int fs_rename(const char *oldpath, const char *newpath)
{
    /* Only allow renaming overlay-backed entries (packaged files are read-only). */
    int oi = overlay_find(oldpath);
    if (oi < 0) return FS_ENOENT;
    /* ensure no existing destination */
    int di = overlay_find(newpath);
    if (di >= 0) return FS_EINVAL;
    /* update overlay name */
    char *newname = kstrdup(newpath);
    if (!newname) return FS_EIO;
    kfree(overlay[oi].name);
    overlay[oi].name = newname;
    /* update node tree if present */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(oldpath);
    if (n) {
        /* detach from old parent */
        detach_node(n);
        /* set new local name and attach under new parent */
        /* derive new parent path and basename */
        char comps[32][128];
        int c = path_to_components(newpath, comps, 32);
        if (c <= 0) {
            /* renaming to root not allowed */
            return FS_EINVAL;
        }
        char *basename = comps[c-1];
        /* find or create parent node */
        struct ram_node *parent = ram_root;
        for (int i = 0; i < c-1; ++i) {
            struct ram_node *ch = find_child(parent, comps[i]);
            if (!ch) {
                /* cannot create intermediate dirs during rename */
                /* restore original parent by inserting back under old path */
                insert_child(parent, basename, n->is_dir);
                return FS_EINVAL;
            }
            parent = ch;
        }
        /* update name */
        if (n->name) kfree(n->name);
        n->name = kstrdup(basename);
        /* attach */
        n->next_sibling = parent->first_child;
        parent->first_child = n;
        n->parent = parent;
    }
    return FS_OK;
}

int fs_truncate(const char *path, size_t size)
{
    int oi = overlay_find(path);
    if (oi >= 0) {
        if (size == overlay[oi].size) return FS_OK;
        if (size == 0) {
            kfree(overlay[oi].data);
            overlay[oi].data = NULL;
            overlay[oi].size = 0;
            /* update node if present */
            build_tree_from_initrd_if_needed();
            struct ram_node *n = find_node_by_path(path);
            if (n) { n->data = NULL; n->size = 0; }
            return FS_OK;
        }
        uint8_t *nptr = krealloc(overlay[oi].data, overlay[oi].size, size);
        if (!nptr) return FS_EIO;
        if (size > overlay[oi].size) memset(nptr + overlay[oi].size, 0, size - overlay[oi].size);
        overlay[oi].data = nptr;
        overlay[oi].size = size;
        build_tree_from_initrd_if_needed();
        struct ram_node *rn = find_node_by_path(path);
        if (rn) { rn->data = overlay[oi].data; rn->size = overlay[oi].size; }
        return FS_OK;
    }
    /* not an overlay file: create overlay copy from packaged and then truncate */
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    int idx = overlay_alloc(path, f->data, f->size, 0);
    if (idx < 0) return FS_EIO;
    /* Now perform truncate on new overlay entry */
    if (size == 0) {
        kfree(overlay[idx].data);
        overlay[idx].data = NULL;
        overlay[idx].size = 0;
    } else {
        uint8_t *n2 = krealloc(overlay[idx].data, overlay[idx].size, size);
        if (!n2) return FS_EIO;
        if (size > overlay[idx].size) memset(n2 + overlay[idx].size, 0, size - overlay[idx].size);
        overlay[idx].data = n2;
        overlay[idx].size = size;
    }
    insert_overlay_node_for_index(path, idx, 0);
    return FS_OK;
}

int fs_rmdir(const char *path)
{
    /* Use node tree semantics: only allow removing overlay-created directories that are empty. */
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (!n) return FS_ENOENT;
    if (!n->is_dir) return FS_EINVAL;
    /* If node has children, cannot remove */
    if (n->first_child) return FS_EINVAL;
    /* If overlay entry exists, remove it */
    int oi = overlay_find(path);
    if (oi >= 0 && overlay[oi].is_dir) {
        overlay_free(oi);
        /* remove node from tree */
        remove_node(n);
        return FS_OK;
    }
    /* If only packaged directory existed (no overlay), do not allow removal */
    return FS_EINVAL;
}

int fs_is_overlay(const char *path)
{
    if (overlay_find(path) >= 0) return 1;
    build_tree_from_initrd_if_needed();
    struct ram_node *n = find_node_by_path(path);
    if (n && n->data) return 1;
    return 0;
}
