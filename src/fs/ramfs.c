#include "../include/fs.h"
#include "../include/tty.h"
#include "../include/memory.h"
#include "../include/string.h"

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

/* Simple dynamic ramfs overlay structures for Phase 2 */
#define OVERLAY_MAX_FILES 64

struct overlay_file {
    char *name;
    uint8_t *data;
    size_t size;
    int is_dir;
    int used;
};

static struct overlay_file overlay[OVERLAY_MAX_FILES];

static int overlay_find(const char *path)
{
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (overlay[i].used && strcmp(overlay[i].name, path) == 0) return i;
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
            return i;
        }
    }
    return -1;
}

static void overlay_free(int idx)
{
    if (idx < 0 || idx >= OVERLAY_MAX_FILES) return;
    if (!overlay[idx].used) return;
    kfree(overlay[idx].name);
    kfree(overlay[idx].data);
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
    /* sanity check: at least zero files ok */
    return FS_OK;
}

static const struct fs_file *find_file(const char *path)
{
    for (unsigned int i = 0; i < initrd_files_count; ++i) {
        if (strcmp(initrd_files[i].name, path) == 0) return &initrd_files[i];
    }
    /* overlay entries take precedence */
    int oi = overlay_find(path);
    if (oi >= 0) {
        /* construct a temporary fs_file mapping to overlay data */
        static struct fs_file temp;
        temp.name = overlay[oi].name;
        temp.data = overlay[oi].data;
        temp.size = overlay[oi].size;
        return &temp;
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
    return idx >= 0 ? FS_OK : FS_EMFILE;
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
    return FS_OK;
}

int fs_mkdir(const char *path)
{
    /* very small behaviour: create an overlay directory entry */
    int oi = overlay_find(path);
    if (oi >= 0) return FS_OK; /* already exists */
    int idx = overlay_alloc(path, NULL, 0, 1);
    return idx >= 0 ? FS_OK : FS_EMFILE;
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
    const struct fs_file *f = find_file(path);
    if (!f) return FS_ENOENT;
    if (st) {
        st->size = f->size;
        st->is_dir = 0;
    }
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
    size_t plen = strlen(path);
    unsigned int found = 0;
    static struct fs_file temp;

    /* normalize root */
    int root = (plen == 0 || (plen == 1 && path[0] == '/'));

    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (!overlay[i].used) continue;
        const char *name = overlay[i].name;
        /* skip if not under path */
        if (!root) {
            if (strncmp(name, path, plen) != 0) continue;
            if (name[plen] != '/') continue; /* not an immediate child */
            /* child name starts at name+plen+1 until next '/' or '\0' */
        } else {
            /* root: immediate children are those without additional '/' besides leading */
            if (name[0] != '/') continue;
        }
        if (found == index) {
            temp.name = overlay[i].name;
            temp.data = overlay[i].data;
            temp.size = overlay[i].size;
            if (out) *out = &temp;
            return FS_OK;
        }
        found++;
    }
    return FS_ENOENT;
}

int fs_rename(const char *oldpath, const char *newpath)
{
    int oi = overlay_find(oldpath);
    if (oi < 0) return FS_ENOENT;
    /* ensure no existing destination */
    int di = overlay_find(newpath);
    if (di >= 0) return FS_EINVAL;
    /* rename by replacing name string */
    char *newname = kstrdup(newpath);
    if (!newname) return FS_EIO;
    kfree(overlay[oi].name);
    overlay[oi].name = newname;
    return FS_OK;
}

int fs_truncate(const char *path, size_t size)
{
    int oi = overlay_find(path);
    if (oi < 0) return FS_ENOENT;
    if (size == overlay[oi].size) return FS_OK;
    if (size == 0) {
        kfree(overlay[oi].data);
        overlay[oi].data = NULL;
        overlay[oi].size = 0;
        return FS_OK;
    }
    uint8_t *n = krealloc(overlay[oi].data, overlay[oi].size, size);
    if (!n) return FS_EIO;
    /* If extended, zero the new region */
    if (size > overlay[oi].size) {
        memset(n + overlay[oi].size, 0, size - overlay[oi].size);
    }
    overlay[oi].data = n;
    overlay[oi].size = size;
    return FS_OK;
}

int fs_rmdir(const char *path)
{
    /* Only allow removal if no entries have this path as prefix */
    size_t plen = strlen(path);
    for (int i = 0; i < OVERLAY_MAX_FILES; ++i) {
        if (!overlay[i].used) continue;
        if (strncmp(overlay[i].name, path, plen) == 0) {
            /* if exact match and is_dir -> ok, else if child exists -> fail */
            if (overlay[i].name[plen] == '\0' && overlay[i].is_dir) {
                /* remove this dir entry only if no child exists */
                /* check for children */
                for (int j = 0; j < OVERLAY_MAX_FILES; ++j) {
                    if (!overlay[j].used || j == i) continue;
                    if (strncmp(overlay[j].name, path, plen) == 0 && overlay[j].name[plen] == '/') return FS_EINVAL;
                }
                overlay_free(i);
                return FS_OK;
            }
        }
    }
    return FS_ENOENT;
}

int fs_is_overlay(const char *path)
{
    return overlay_find(path) >= 0;
}
