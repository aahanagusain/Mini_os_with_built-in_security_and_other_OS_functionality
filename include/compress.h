#ifndef _COMPRESS_H
#define _COMPRESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run-Length Encoding (RLE) compression
 * Simple but effective for data with many repeated bytes
 * Format: [byte_value][count] repeated
 */

#define COMPRESS_MAX_SIZE 65536
#define COMPRESS_MAX_RUN 255

/* Compress data using RLE algorithm */
int compress_rle(const uint8_t *input, size_t input_len,
                 uint8_t *output, size_t output_len);

/* Decompress data using RLE algorithm */
int decompress_rle(const uint8_t *input, size_t input_len,
                   uint8_t *output, size_t output_len);

/* Compress file: read from source, write compressed to destination */
int compress_file(const char *src_path, const char *dst_path);

/* Decompress file: read from source, write decompressed to destination */
int decompress_file(const char *src_path, const char *dst_path);

#ifdef __cplusplus
}
#endif

#endif /* _COMPRESS_H */
