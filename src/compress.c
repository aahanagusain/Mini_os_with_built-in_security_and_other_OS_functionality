#include "../include/compress.h"
#include "../include/fs.h"
#include "../include/io.h"

/* RLE Compression: [byte_value][count] format */
int compress_rle(const uint8_t *input, size_t input_len,
                 uint8_t *output, size_t output_len)
{
    if (!input || !output) {
        return -1;
    }
    
    if (input_len == 0) {
        return 0;
    }
    
    if (output_len < input_len * 2) {
        return -2;
    }
    
    size_t out_idx = 0;
    size_t in_idx = 0;
    
    while (in_idx < input_len && out_idx < output_len) {
        uint8_t current_byte = input[in_idx];
        uint32_t count = 1;
        
        /* Count consecutive identical bytes */
        while (in_idx + count < input_len &&
               input[in_idx + count] == current_byte &&
               count < COMPRESS_MAX_RUN) {
            count++;
        }
        
        /* Check if output buffer has space for [byte][count] */
        if (out_idx + 2 > output_len) {
            return -3;
        }
        
        /* Write [byte_value][count] */
        output[out_idx++] = current_byte;
        output[out_idx++] = (uint8_t)count;
        
        in_idx += count;
    }
    
    return (int)out_idx;
}

/* RLE Decompression: read [byte_value][count] pairs */
int decompress_rle(const uint8_t *input, size_t input_len,
                   uint8_t *output, size_t output_len)
{
    if (!input || !output) {
        return -1;
    }
    
    if (input_len == 0) {
        return 0;
    }
    
    if (input_len % 2 != 0) {
        return -2; /* Compressed data should have even length */
    }
    
    size_t out_idx = 0;
    size_t in_idx = 0;
    
    while (in_idx < input_len && out_idx < output_len) {
        uint8_t byte_value = input[in_idx];
        uint8_t count = input[in_idx + 1];
        
        in_idx += 2;
        
        /* Expand: write 'count' copies of 'byte_value' */
        for (uint8_t i = 0; i < count && out_idx < output_len; i++) {
            output[out_idx++] = byte_value;
        }
    }
    
    return (int)out_idx;
}

/* Compress entire file */
int compress_file(const char *src_path, const char *dst_path)
{
    if (!src_path || !dst_path) {
        return -1;
    }
    
    /* Open source file for reading */
    int src_fd = fs_open(src_path, FS_O_RDONLY);
    if (src_fd < 0) {
        return -2;
    }
    
    /* Create destination file */
    int c = fs_create(dst_path, (const uint8_t *)"", 0);
    if (c != FS_OK) {
        fs_close(src_fd);
        return -3;
    }
    
    int dst_fd = fs_open(dst_path, FS_O_RDONLY);
    if (dst_fd < 0) {
        fs_close(src_fd);
        return -3;
    }
    
    /* Buffer for file I/O */
    uint8_t input_buf[1024];
    uint8_t output_buf[2048]; /* RLE can expand data */
    int total_compressed = 0;
    
    /* Read and compress in chunks */
    int bytes_read;
    while (1) {
        bytes_read = fs_read(src_fd, input_buf, sizeof(input_buf));
        if (bytes_read < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -4;
        }
        if (bytes_read == 0) {
            break;
        }
        
        /* Compress this chunk */
        int compressed_len = compress_rle(input_buf, bytes_read, output_buf, sizeof(output_buf));
        if (compressed_len < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -5;
        }
        
        /* Write compressed data */
        int written = fs_write(dst_fd, output_buf, compressed_len);
        if (written < 0 || written != compressed_len) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -6;
        }
        
        total_compressed += written;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    return total_compressed;
}

/* Decompress entire file */
int decompress_file(const char *src_path, const char *dst_path)
{
    if (!src_path || !dst_path) {
        return -1;
    }
    
    /* Open source file for reading */
    int src_fd = fs_open(src_path, FS_O_RDONLY);
    if (src_fd < 0) {
        return -2;
    }
    
    /* Create destination file */
    int c = fs_create(dst_path, (const uint8_t *)"", 0);
    if (c != FS_OK) {
        fs_close(src_fd);
        return -3;
    }
    
    int dst_fd = fs_open(dst_path, FS_O_RDONLY);
    if (dst_fd < 0) {
        fs_close(src_fd);
        return -3;
    }
    
    /* Buffer for file I/O */
    uint8_t input_buf[2048];  /* Compressed data */
    uint8_t output_buf[4096]; /* Decompressed data can be larger */
    int total_decompressed = 0;
    
    /* Read and decompress in chunks */
    int bytes_read;
    while (1) {
        bytes_read = fs_read(src_fd, input_buf, sizeof(input_buf));
        if (bytes_read < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -4;
        }
        if (bytes_read == 0) {
            break;
        }
        
        /* Decompress this chunk */
        int decompressed_len = decompress_rle(input_buf, bytes_read, output_buf, sizeof(output_buf));
        if (decompressed_len < 0) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -5;
        }
        
        /* Write decompressed data */
        int written = fs_write(dst_fd, output_buf, decompressed_len);
        if (written < 0 || written != decompressed_len) {
            fs_close(src_fd);
            fs_close(dst_fd);
            return -6;
        }
        
        total_decompressed += written;
    }
    
    fs_close(src_fd);
    fs_close(dst_fd);
    
    return total_decompressed;
}
