#include "../include/user.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/memory.h"
#include "../include/sha256.h"
#include <stdbool.h>

/* Very small fixed-size user table */
#define MAX_USERS 16

static user_t users[MAX_USERS];
static int user_count = 0;
static int current = -1;

/* helper: parse a line of form name:passwd:uid:gid */
static void parse_line(char *line)
{
    if (!line) return;
    char *p = line;
    char *fields[4] = {0};
    int f = 0;
    while (*p && f < 4) {
        fields[f++] = p;
        while (*p && *p != ':') p++;
        if (*p == ':') { *p = '\0'; p++; }
    }
    if (f < 2) return;
    if (user_count >= MAX_USERS) return;
    user_t *u = &users[user_count++];
    strncpy(u->name, fields[0], USER_NAME_MAX-1);
    u->name[USER_NAME_MAX-1] = '\0';
    if (fields[1]) {
        strncpy(u->passwd, fields[1], USER_PASS_MAX-1);
        u->passwd[USER_PASS_MAX-1] = '\0';
    } else {
        u->passwd[0] = '\0';
    }
    u->uid = 0; u->gid = 0;
    if (fields[2]) u->uid = (unsigned int)atoi(fields[2]);
    if (fields[3]) u->gid = (unsigned int)atoi(fields[3]);
}

int user_init_from_file(const char *path)
{
    /* attempt to open file from embedded initrd */
    fs_fd_t fd = fs_open(path, FS_O_RDONLY);
    if (fd < 0) return -1;
    /* read entire file into buffer */
    /* stat to get size */
    struct fs_stat st;
    if (fs_stat(path, &st) != FS_OK) { fs_close(fd); return -1; }
    size_t sz = st.size;
    char *buf = (char *)kmalloc(sz + 1);
    if (!buf) { fs_close(fd); return -1; }
    int r = fs_read(fd, buf, sz);
    buf[sz] = '\0';
    fs_close(fd);
    if (r <= 0) { kfree(buf); return -1; }

    /* split into lines */
    char *s = buf;
    while (*s) {
        char *nl = strchr(s, '\n');
        if (nl) { *nl = '\0'; }
        /* ignore comments and empty lines */
        if (*s && *s != '#') parse_line(s);
        if (!nl) break;
        s = nl + 1;
    }
    kfree(buf);
    return 0;
}

int user_login(const char *name, const char *password)
{
    if (!name) return -1;
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            /* detect stored SHA-256 hex digest (64 lowercase hex chars) */
            const char *stored = users[i].passwd;
            size_t sl = strlen(stored);
            if (sl == 64) {
                bool ishex = true;
                for (size_t j = 0; j < 64; ++j) {
                    char c = stored[j];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { ishex = false; break; }
                }
                if (ishex) {
                    char out[65];
                    if (sha256_hex(password, out, sizeof(out)) == 0 && strcmp(out, stored) == 0) {
                        current = i; return 0;
                    }
                    return -1;
                }
            }
            /* fallback: plaintext compare for backwards compatibility */
            if (strcmp(stored, password) == 0) {
                current = i; return 0;
            }
            return -1;
        }
    }
    return -1;
}

const user_t *user_current(void)
{
    if (current < 0 || current >= user_count) return NULL;
    return &users[current];
}

void user_logout(void)
{
    current = -1;
}

int user_is_root(void)
{
    if (current < 0 || current >= user_count) return 0;
    return (users[current].uid == 0) ? 1 : 0;
}

const user_t *user_get_by_name(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

int user_add(const char *name, const char *password)
{
    if (!name || !password) return -1;
    if (user_count >= MAX_USERS) return -2;
    
    /* Check if user already exists */
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].name, name) == 0) return -3;
    }
    
    /* Add new user */
    user_t *u = &users[user_count++];
    strncpy(u->name, name, USER_NAME_MAX-1);
    u->name[USER_NAME_MAX-1] = '\0';
    
    /* Hash password with SHA-256 */
    char hash[65];
    if (sha256_hex(password, hash, sizeof(hash)) != 0) {
        user_count--;
        return -4;
    }
    strncpy(u->passwd, hash, USER_PASS_MAX-1);
    u->passwd[USER_PASS_MAX-1] = '\0';
    
    /* Assign uid/gid automatically */
    u->uid = user_count;  /* Simple auto-increment */
    u->gid = user_count;
    
    return 0;
}

int user_switch(const char *name, const char *password)
{
    if (!name || !password) return -1;
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            const char *stored = users[i].passwd;
            size_t sl = strlen(stored);
            if (sl == 64) {
                bool ishex = true;
                for (size_t j = 0; j < 64; ++j) {
                    char c = stored[j];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { 
                        ishex = false; 
                        break; 
                    }
                }
                if (ishex) {
                    char out[65];
                    if (sha256_hex(password, out, sizeof(out)) == 0 && strcmp(out, stored) == 0) {
                        current = i;
                        return 0;
                    }
                    return -1;
                }
            }
            if (strcmp(stored, password) == 0) {
                current = i;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

void user_list_all(void)
{
    for (int i = 0; i < user_count; ++i) {
        char marker = (i == current) ? '*' : ' ';
        printk("\n  %c %s (uid:%u gid:%u)", marker, users[i].name, users[i].uid, users[i].gid);
    }
    if (user_count == 0) {
        printk("\n  (no users)");
    }
}

int user_delete(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].name, name) == 0) {
            /* Don't allow deleting current user */
            if (i == current) return -2;
            /* Shift users down manually */
            if (i < user_count - 1) {
                for (int j = i; j < user_count - 1; j++) {
                    users[j] = users[j + 1];
                }
            }
            user_count--;
            /* Adjust current index if needed */
            if (current > i) current--;
            return 0;
        }
    }
    return -1;
}
