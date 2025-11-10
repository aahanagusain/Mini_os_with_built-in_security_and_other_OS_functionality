/* Minimal user management API (Phase 1)
 * - Parse /etc/passwd-like file from initrd
 * - Provide login and current-user query
 */
#ifndef USER_H
#define USER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USER_NAME_MAX 32
#define USER_PASS_MAX 64

typedef struct user {
    char name[USER_NAME_MAX];
    char passwd[USER_PASS_MAX]; /* plaintext or hashed depending on system */
    unsigned int uid;
    unsigned int gid;
} user_t;

/* Initialize user subsystem by reading a passwd file in initrd. Returns 0 on success. */
int user_init_from_file(const char *path);

/* Attempt login with username and password. Returns 0 on success, non-zero otherwise. */
int user_login(const char *name, const char *password);

/* Get currently logged-in user or NULL if none. */
const user_t *user_current(void);

/* Log out current user */
void user_logout(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
