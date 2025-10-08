#include "../include/fs.h"
#include "../include/tty.h"
#include <string.h>

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
    /* sanity check: at least zero files ok */
    return FS_OK;
}

static const struct fs_file *find_file(const char *path)
{
    for (unsigned int i = 0; i < initrd_files_count; ++i) {
        if (strcmp(initrd_files[i].name, path) == 0) return &initrd_files[i];
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
    if (index >= initrd_files_count) return FS_ENOENT;
    if (out) *out = &initrd_files[index];
    return FS_OK;
}
