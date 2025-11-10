#ifndef _SHA256_H
#define _SHA256_H 1

#include <stddef.h>

/* Compute SHA-256 digest for `message` and write a 64-character
 * lowercase hex string into `out` (must have at least 65 bytes).
 * Returns 0 on success. */
int sha256_hex(const char *message, char *out, size_t out_len);

/* Legacy helper that prints the digest to the kernel console. Kept
 * for compatibility with existing callers. */
void sha256(const char *message);

#endif