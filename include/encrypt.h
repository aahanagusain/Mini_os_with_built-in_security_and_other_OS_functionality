#ifndef _ENCRYPT_H
#define _ENCRYPT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Simple XOR cipher for basic encryption (educational use)
 * Uses a key-based XOR cipher for file encryption
 */

#define ENCRYPT_KEY_MAX 256
#define ENCRYPT_MAX_SIZE 65536

/* Encrypt data with XOR cipher using provided key */
int encrypt_xor(const uint8_t *input, size_t input_len,
                uint8_t *output, size_t output_len,
                const char *key, size_t key_len);

/* Decrypt data with XOR cipher using provided key */
int decrypt_xor(const uint8_t *input, size_t input_len,
                uint8_t *output, size_t output_len,
                const char *key, size_t key_len);

/* Encrypt file: read from source, write encrypted to destination */
int encrypt_file(const char *src_path, const char *dst_path, const char *key);

/* Decrypt file: read from source, write decrypted to destination */
int decrypt_file(const char *src_path, const char *dst_path, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* _ENCRYPT_H */
