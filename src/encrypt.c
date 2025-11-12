#include "encrypt.h"
#include "fs.h"
#include "string.h"
#include "io.h"

/* XOR encryption with key cycling */
int encrypt_xor(const uint8_t *input, size_t input_len,
                uint8_t *output, size_t output_len,
                const char *key, size_t key_len)
{
    if (!input || !output || !key || key_len == 0) {
        return -1;
    }
    
    if (output_len < input_len) {
        return -2;
    }
    
    /* XOR each byte with corresponding key byte (cycling through key) */
    for (size_t i = 0; i < input_len; i++) {
        output[i] = input[i] ^ (uint8_t)key[i % key_len];
    }
    
    return (int)input_len;
}

/* XOR decryption (same as encryption due to XOR properties) */
int decrypt_xor(const uint8_t *input, size_t input_len,
                uint8_t *output, size_t output_len,
                const char *key, size_t key_len)
{
    /* Decryption is identical to encryption for XOR cipher */
    return encrypt_xor(input, input_len, output, output_len, key, key_len);
}

/* Encrypt entire file */
int encrypt_file(const char *src_path, const char *dst_path, const char *key)
{
    if (!src_path || !dst_path || !key) {
        return -1;
    }
    
    size_t key_len = strlen(key);
    if (key_len == 0 || key_len > ENCRYPT_KEY_MAX) {
        return -2;
    }
    
    /* Open source file for reading */
    int src_fd = fs_open(src_path, FS_O_RDONLY);
    if (src_fd < 0) {
        return -3;
    }
    
    /* Create destination file (write via fs_create then fs_open/fs_write) */
    int c = fs_create(dst_path, (const uint8_t *)"", 0);
    if (c != FS_OK) {
        fs_close(src_fd);
        return -4;
    }
    
    int dst_fd = fs_open(dst_path, FS_O_RDONLY);
    if (dst_fd < 0) {
        fs_close(src_fd);
        return -4;
    }
    
    /* Buffer for file I/O */
    uint8_t input_buf[1024];
    uint8_t output_buf[1024];
    int total_encrypted = 0;
    
    /* Read and encrypt in chunks */
    int bytes_read;
    while (1) {
        bytes_read = fs_read(src_fd, input_buf, sizeof(input_buf));
        if (bytes_read < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -5;
        }
        if (bytes_read == 0) {
            break;
        }
        
        /* Encrypt this chunk */
        int encrypted_len = encrypt_xor(input_buf, bytes_read, output_buf, sizeof(output_buf),
                                       key, key_len);
        if (encrypted_len < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -6;
        }
        
        /* Write encrypted data */
        int written = fs_write(dst_fd, output_buf, encrypted_len);
        if (written < 0 || written != encrypted_len) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -7;
        }
        
        total_encrypted += written;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    return total_encrypted;
}

/* Decrypt entire file */
int decrypt_file(const char *src_path, const char *dst_path, const char *key)
{
    if (!src_path || !dst_path || !key) {
        return -1;
    }
    
    size_t key_len = strlen(key);
    if (key_len == 0 || key_len > ENCRYPT_KEY_MAX) {
        return -2;
    }
    
    /* Open source file for reading */
    int src_fd = fs_open(src_path, FS_O_RDONLY);
    if (src_fd < 0) {
        return -3;
    }
    
    /* Create destination file */
    int c = fs_create(dst_path, (const uint8_t *)"", 0);
    if (c != FS_OK) {
        fs_close(src_fd);
        return -4;
    }
    
    int dst_fd = fs_open(dst_path, FS_O_RDONLY);
    if (dst_fd < 0) {
        fs_close(src_fd);
        return -4;
    }
    
    /* Buffer for file I/O */
    uint8_t input_buf[1024];
    uint8_t output_buf[1024];
    int total_decrypted = 0;
    
    /* Read and decrypt in chunks */
    int bytes_read;
    while (1) {
        bytes_read = fs_read(src_fd, input_buf, sizeof(input_buf));
        if (bytes_read < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -5;
        }
        if (bytes_read == 0) {
            break;
        }
        
        /* Decrypt this chunk (same as encrypt for XOR) */
        int decrypted_len = decrypt_xor(input_buf, bytes_read, output_buf, sizeof(output_buf),
                                        key, key_len);
        if (decrypted_len < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -6;
        }
        
        /* Write decrypted data */
        int written = fs_write(dst_fd, output_buf, decrypted_len);
        if (written < 0 || written != decrypted_len) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -7;
        }
        
        total_decrypted += written;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    return total_decrypted;
}
