#include "../include/sha256.h"
#include "../include/crypto.h"
#include "../include/string.h"
#include "../include/memory.h"
#include "../include/tty.h"

/* Minimal SHA-256 implementation that supports single-block messages
 * (messages up to 55 bytes). This is sufficient for hashing passwords
 * in this small kernel. For longer messages the function will return -1.
 */
static int sha256_compute(const char *message, uint32_t out[8])
{
    if (!message || !out) return -1;
    uint32_t h0 = 0x6a09e667;
    uint32_t h1 = 0xbb67ae85;
    uint32_t h2 = 0x3c6ef372;
    uint32_t h3 = 0xa54ff53a;
    uint32_t h4 = 0x510e527f;
    uint32_t h5 = 0x9b05688c;
    uint32_t h6 = 0x1f83d9ab;
    uint32_t h7 = 0x5be0cd19;

    uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    size_t len = strlen(message);
    if (len > 55) return -1;

    uint8_t block[64];
    memset(block, 0, sizeof(block));
    memcpy(block, message, len);
    block[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i) block[63 - i] = (uint8_t)(bitlen >> (i * 8));

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) | ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = _rotr(w[i-15], 7) ^ _rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = _rotr(w[i-2], 17) ^ _rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = _rotr(e,6) ^ _rotr(e,11) ^ _rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + k[i] + w[i];
        uint32_t S0 = _rotr(a,2) ^ _rotr(a,13) ^ _rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    h0 += a; h1 += b; h2 += c; h3 += d; h4 += e; h5 += f; h6 += g; h7 += h;

    out[0] = h0; out[1] = h1; out[2] = h2; out[3] = h3; out[4] = h4; out[5] = h5; out[6] = h6; out[7] = h7;
    return 0;
}

int sha256_hex(const char *message, char *out, size_t out_len)
{
    if (!message || !out || out_len < 65) return -1;
    uint32_t digest[8];
    if (sha256_compute(message, digest) != 0) return -1;
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 8; ++i) {
        uint32_t v = digest[i];
        for (int b = 0; b < 4; ++b) {
            uint8_t byte = (v >> (24 - b*8)) & 0xFF;
            out[i*8 + b*2 + 0] = hex[(byte >> 4) & 0xF];
            out[i*8 + b*2 + 1] = hex[byte & 0xF];
        }
    }
    out[64] = '\0';
    return 0;
}

void sha256(const char *message)
{
    char hex[65];
    if (sha256_hex(message, hex, sizeof(hex)) == 0) {
        printk("\n%s", hex);
    } else {
        printk("\n<sha256 error>");
    }
}