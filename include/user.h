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

/* Add a new user with SHA-256 hashed password. Returns 0 on success. */
int user_add(const char *name, const char *password);

/* Check if current user is root (uid 0). Returns 1 if root, 0 otherwise. */
int user_is_root(void);

/* Get user by name. Returns user_t pointer or NULL if not found. */
const user_t *user_get_by_name(const char *name);

/* Change current user to another (for su command). Returns 0 on success. */
int user_switch(const char *name, const char *password);

/* List all users. */
void user_list_all(void);

/* Delete user by name. Returns 0 on success. */
int user_delete(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
