/* Simple filesystem API for embedded initrd (Phase 1 ramfs)
 * Provides a minimal open/read/close interface for files compiled into the kernel.
 */
#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FS_O_RDONLY 0x1

typedef int fs_fd_t;
enum fs_err { FS_OK = 0, FS_ENOENT = -1, FS_EIO = -2, FS_EINVAL = -3, FS_EMFILE = -4 };

struct fs_file {
    const char *name;       /* null-terminated path, e.g. "/README.txt" */
    const uint8_t *data;   /* pointer to contents */
    size_t size;           /* size in bytes */
};

struct fs_stat {
    size_t size;    /* file size in bytes */
    int is_dir;     /* 0 = file, 1 = directory (not used by ramfs) */
};

/* Mount the embedded initrd produced by tools/mkinitrd.py. This will make
 * the symbol `initrd_files`/`initrd_files_count` available to the ramfs
 * implementation and initialise the file descriptor table. Returns FS_OK on
 * success. */
int fs_mount_initrd_embedded(void);

/* Minimal file operations */
fs_fd_t fs_open(const char *path, int flags);
int fs_read(fs_fd_t fd, void *buf, size_t count);
int fs_close(fs_fd_t fd);

/* Query metadata for a path. Returns FS_OK or FS_ENOENT */
int fs_stat(const char *path, struct fs_stat *st);

/* Enumerate initrd entries by zero-based index. On success returns FS_OK
 * and sets *out to point to the internal fs_file entry. When index is out of
 * range returns FS_ENOENT. This is a simple helper useful for mounting and
 * listing contents; it is intentionally minimal for Phase 1. */
int fs_readdir(unsigned int index, const struct fs_file **out);

/* Phase 2: writable in-memory file operations (simple ramfs overlay).
 * These are minimal helpers to create, write (append/overwrite), unlink
 * and create directories in a tiny dynamic ramfs that lives in kernel
 * memory. These do not persist across reboots.
 */
int fs_create(const char *path, const uint8_t *data, size_t size);
int fs_write(fs_fd_t fd, const void *buf, size_t count); /* append/write to an open fd */
int fs_unlink(const char *path);
int fs_mkdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* FS_H */

