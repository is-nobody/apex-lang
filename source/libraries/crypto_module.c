// source/libraries/crypto_module.c
// Implementation of Crypto Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "crypto_module.h"
#include "vm.h"
#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// fills a buffer with cryptographically secure random bytes
static void get_secure_bytes(unsigned char* buffer, size_t length) {
#if defined(_WIN32)
    for (size_t i = 0; i < length; i++) {         // iterate over buffer
        unsigned int val;                         // random value
        rand_s(&val);                             // get secure random
        buffer[i] = (unsigned char)(val & 0xFF);  // store low byte
    }
#else
    FILE* f = fopen("/dev/urandom", "rb");                        // open urandom
    if (f) {                                                      // opened successfully
        size_t read_count = fread(buffer, 1, length, f);          // read random bytes
        fclose(f);                                                // close file
        if (read_count == length) return;                         // success
    }
    static int seeded = 0;                                        // seed flag
    if (!seeded) {                                                // not seeded
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());  // seed from time
        seeded = 1;                                               // mark seeded
    }
    for (size_t i = 0; i < length; i++)                           // fallback to rand
        buffer[i] = (unsigned char)(rand() % 256);                // fill with rand
#endif
}

// converts bytes to a hex string
static void bytes_to_hex(const unsigned char* bytes, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";  // hex characters
    for (size_t i = 0; i < len; i++) {                   // iterate over bytes
        out[i * 2] = hex_chars[(bytes[i] >> 4) & 0xF];   // high nibble
        out[i * 2 + 1] = hex_chars[bytes[i] & 0xF];      // low nibble
    }
    out[len * 2] = '\0';                                 // null terminate
}

// constant-time comparison to prevent timing attacks
static bool constant_time_compare(const char* a, const char* b, size_t len_a, size_t len_b) {
    if (len_a != len_b) return false;                         // different lengths
    volatile unsigned char result = 0;                        // accumulator
    for (size_t i = 0; i < len_a; i++) {                      // iterate over bytes
        result |= (unsigned char)a[i] ^ (unsigned char)b[i];  // xor and accumulate
    }
    return result == 0;                                       // all bytes matched
}

// hex string to bytes conversion
static int hex_to_bytes(const char* hex, unsigned char* bytes, int max_len) {
    int len = (int)strlen(hex);
    if (len % 2 != 0) return -1;  // invalid hex length
    
    int byte_len = len / 2;
    if (byte_len > max_len) return -1;  // too long
    
    for (int i = 0; i < byte_len; i++) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];
        
        if (high >= '0' && high <= '9') bytes[i] = (unsigned char)((high - '0') << 4);
        else if (high >= 'a' && high <= 'f') bytes[i] = (unsigned char)((high - 'a' + 10) << 4);
        else if (high >= 'A' && high <= 'F') bytes[i] = (unsigned char)((high - 'A' + 10) << 4);
        else return -1;
        
        if (low >= '0' && low <= '9') bytes[i] |= (unsigned char)(low - '0');
        else if (low >= 'a' && low <= 'f') bytes[i] |= (unsigned char)(low - 'a' + 10);
        else if (low >= 'A' && low <= 'F') bytes[i] |= (unsigned char)(low - 'A' + 10);
        else return -1;
    }
    
    return byte_len;
}

typedef void (*hash_init_fn)(void*);
typedef void (*hash_update_fn)(void*, const unsigned char*, unsigned int);
typedef void (*hash_final_fn)(unsigned char*, void*);

typedef struct {
    uint32_t state[4];         // state (ABCD)
    uint32_t count[2];         // number of bits, modulo 2^64
    unsigned char buffer[64];  // input buffer
} MD5_CTX;

// md5 shift amounts
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

// f, g, h and i are basic md5 functions
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))  // round 1 function
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))  // round 2 function
#define H(x, y, z) ((x) ^ (y) ^ (z))             // round 3 function
#define I(x, y, z) ((y) ^ ((x) | (~z)))          // round 4 function

// rotates x left n bits
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4
#define FF(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define GG(a, b, c, d, x, s, ac) { \
    (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define HH(a, b, c, d, x, s, ac) { \
    (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define II(a, b, c, d, x, s, ac) { \
    (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

// encode bytes into uint32_t (little-endian)
static void md5_encode(uint32_t* output, const unsigned char* input, unsigned int len) {
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4) {  // process each word
        output[i] = (uint32_t)input[j] |                     // first byte
                   ((uint32_t)input[j+1] << 8) |             // second byte
                   ((uint32_t)input[j+2] << 16) |            // third byte
                   ((uint32_t)input[j+3] << 24);             // fourth byte
    }
}

// decode uint32_t into bytes (little-endian)
static void md5_decode(unsigned char* output, const uint32_t* input, unsigned int len) {
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4) {      // process each word
        output[j]   = (unsigned char)(input[i] & 0xFF);          // first byte
        output[j+1] = (unsigned char)((input[i] >> 8) & 0xFF);   // second byte
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xFF);  // third byte
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xFF);  // fourth byte
    }
}

// md5 basic transformation
static void md5_transform(uint32_t state[4], const unsigned char block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];  // copy state
    uint32_t x[16];                                                   // block words
    md5_encode(x, block, 64);                                         // decode block

    // round 1
    FF(a, b, c, d, x[ 0], S11, 0xd76aa478); // 1
    FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); // 2
    FF(c, d, a, b, x[ 2], S13, 0x242070db); // 3
    FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); // 4
    FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); // 5
    FF(d, a, b, c, x[ 5], S12, 0x4787c62a); // 6
    FF(c, d, a, b, x[ 6], S13, 0xa8304613); // 7
    FF(b, c, d, a, x[ 7], S14, 0xfd469501); // 8
    FF(a, b, c, d, x[ 8], S11, 0x698098d8); // 9
    FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); // 10
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); // 11
    FF(b, c, d, a, x[11], S14, 0x895cd7be); // 12
    FF(a, b, c, d, x[12], S11, 0x6b901122); // 13
    FF(d, a, b, c, x[13], S12, 0xfd987193); // 14
    FF(c, d, a, b, x[14], S13, 0xa679438e); // 15
    FF(b, c, d, a, x[15], S14, 0x49b40821); // 16

    // round 2
    GG(a, b, c, d, x[ 1], S21, 0xf61e2562); // 17
    GG(d, a, b, c, x[ 6], S22, 0xc040b340); // 18
    GG(c, d, a, b, x[11], S23, 0x265e5a51); // 19
    GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); // 20
    GG(a, b, c, d, x[ 5], S21, 0xd62f105d); // 21
    GG(d, a, b, c, x[10], S22,  0x2441453); // 22
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); // 23
    GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); // 24
    GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); // 25
    GG(d, a, b, c, x[14], S22, 0xc33707d6); // 26
    GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); // 27
    GG(b, c, d, a, x[ 8], S24, 0x455a14ed); // 28
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); // 29
    GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); // 30
    GG(c, d, a, b, x[ 7], S23, 0x676f02d9); // 31
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); // 32

    // round 3
    HH(a, b, c, d, x[ 5], S31, 0xfffa3942); // 33
    HH(d, a, b, c, x[ 8], S32, 0x8771f681); // 34
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); // 35
    HH(b, c, d, a, x[14], S34, 0xfde5380c); // 36
    HH(a, b, c, d, x[ 1], S31, 0xa4beea44); // 37
    HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); // 38
    HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); // 39
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); // 40
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); // 41
    HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); // 42
    HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); // 43
    HH(b, c, d, a, x[ 6], S34,  0x4881d05); // 44
    HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); // 45
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); // 46
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); // 47
    HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); // 48

    // round 4
    II(a, b, c, d, x[ 0], S41, 0xf4292244); // 49
    II(d, a, b, c, x[ 7], S42, 0x432aff97); // 50
    II(c, d, a, b, x[14], S43, 0xab9423a7); // 51
    II(b, c, d, a, x[ 5], S44, 0xfc93a039); // 52
    II(a, b, c, d, x[12], S41, 0x655b59c3); // 53
    II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); // 54
    II(c, d, a, b, x[10], S43, 0xffeff47d); // 55
    II(b, c, d, a, x[ 1], S44, 0x85845dd1); // 56
    II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); // 57
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); // 58
    II(c, d, a, b, x[ 6], S43, 0xa3014314); // 59
    II(b, c, d, a, x[13], S44, 0x4e0811a1); // 60
    II(a, b, c, d, x[ 4], S41, 0xf7537e82); // 61
    II(d, a, b, c, x[11], S42, 0xbd3af235); // 62
    II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); // 63
    II(b, c, d, a, x[ 9], S44, 0xeb86d391); // 64

    state[0] += a;  // update state
    state[1] += b;  // update state
    state[2] += c;  // update state
    state[3] += d;  // update state
}

// initialize md5 context
static void md5_init(MD5_CTX* context) {
    context->count[0] = context->count[1] = 0;  // clear bit count
    context->state[0] = 0x67452301;             // initial hash value A
    context->state[1] = 0xefcdab89;             // initial hash value B
    context->state[2] = 0x98badcfe;             // initial hash value C
    context->state[3] = 0x10325476;             // initial hash value D
}

// md5 block update operation
static void md5_update(MD5_CTX* context, const unsigned char* input, unsigned int input_len) {
    unsigned int i, index, part_len;                                 // counters

    index = (unsigned int)((context->count[0] >> 3) & 0x3F);         // byte index
    context->count[0] += ((uint32_t)input_len << 3);                 // update bit count
    if (context->count[0] < ((uint32_t)input_len << 3))              // overflow
        context->count[1]++;                                         // carry
    context->count[1] += ((uint32_t)input_len >> 29);                // update high bits
    part_len = 64 - index;                                           // remaining space

    if (input_len >= part_len) {                                     // transform as needed
        memcpy(&context->buffer[index], input, part_len);            // fill buffer
        md5_transform(context->state, context->buffer);              // transform
        for (i = part_len; i + 63 < input_len; i += 64)              // transform blocks
            md5_transform(context->state, &input[i]);                // transform
        index = 0;                                                   // reset index
    } else {
        i = 0;                                                       // no transform
    }
    memcpy(&context->buffer[index], &input[i], input_len - i);       // buffer remaining
}

// md5 finalization
static void md5_final(unsigned char digest[16], MD5_CTX* context) {
    unsigned char bits[8];                                           // bit count
    unsigned int index, pad_len;                                     // padding

    md5_encode((uint32_t*)bits, (unsigned char*)context->count, 8);  // encode count
    index = (unsigned int)((context->count[0] >> 3) & 0x3f);         // index
    pad_len = (index < 56) ? (56 - index) : (120 - index);           // padding length
    static const unsigned char padding[64] = {0x80};                 // padding
    md5_update(context, padding, pad_len);                           // pad
    md5_update(context, bits, 8);                                    // append length
    md5_decode(digest, context->state, 16);                          // store state
}

typedef struct {
    uint32_t state[5];        // state (abcde)
    uint32_t count[2];        // number of bits, modulo 2^64
    unsigned char buffer[64]; // input buffer
} SHA1_CTX;

// sha1 rotate left macro
#define SHA1_ROTL(x, n) ROTATE_LEFT(x, n)

// sha1 basic transformation
static void sha1_transform(uint32_t state[5], const unsigned char buffer[64]) {
    uint32_t a, b, c, d, e;                                   // working variables
    uint32_t w[80];                                           // message schedule
    int i;                                                    // loop counter

    for (i = 0; i < 16; i++) {                                // copy block into w[0..15]
        w[i] = ((uint32_t)buffer[i * 4] << 24) |              // big-endian decode
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               (uint32_t)buffer[i * 4 + 3];
    }

    for (i = 16; i < 80; i++) {                               // extend into w[16..79]
        uint32_t temp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];  // xor four words
        w[i] = SHA1_ROTL(temp, 1);                            // rotate left by 1
    }

    a = state[0];                                             // load state
    b = state[1];                                             // load state
    c = state[2];                                             // load state
    d = state[3];                                             // load state
    e = state[4];                                             // load state

    for (i = 0; i < 80; i++) {                                // main loop
        uint32_t f, k, temp;                                  // round function and constant

        if (i < 20) {                                         // rounds 0-19
            f = (b & c) | ((~b) & d);                         // vh(b, c, d) = (b ∧ c) ⊕ (¬b ∧ d)
            k = 0x5A827999;                                   // k0 = ⌊2^30 × √2⌋
        } else if (i < 40) {                                  // rounds 20-39
            f = b ^ c ^ d;                                    // parity(b, c, d) = b ⊕ c ⊕ d
            k = 0x6ED9EBA1;                                   // k1 = ⌊2^30 × √3⌋
        } else if (i < 60) {                                  // rounds 40-59
            f = (b & c) | (b & d) | (c & d);                  // maj(b, c, d) = (b ∧ c) ∨ (b ∧ d) ∨ (c ∧ d)
            k = 0x8F1BBCDC;                                   // k2 = ⌊2^30 × √5⌋
        } else {                                              // rounds 60-79
            f = b ^ c ^ d;                                    // parity(b, c, d) = b ⊕ c ⊕ d
            k = 0xCA62C1D6;                                   // k3 = ⌊2^30 × √10⌋
        }

        temp = SHA1_ROTL(a, 5) + f + e + k + w[i];  // compute temp
        e = d;                                      // shift e
        d = c;                                      // shift d
        c = SHA1_ROTL(b, 30);                       // shift c with rotation
        b = a;                                      // shift b
        a = temp;                                   // shift a
    }

    state[0] += a;  // update state
    state[1] += b;  // update state
    state[2] += c;  // update state
    state[3] += d;  // update state
    state[4] += e;  // update state
}

// initialize sha1 context
static void sha1_init(SHA1_CTX* context) {
    context->count[0] = context->count[1] = 0;  // clear bit count
    context->state[0] = 0x67452301;             // initial hash values (IVs)
    context->state[1] = 0xEFCDAB89;             // derived from first 32 bits of
    context->state[2] = 0x98BADCFE;             // fractional parts of square roots
    context->state[3] = 0x10325476;             // of primes 2, 3, 5, 7, and 11
    context->state[4] = 0xC3D2E1F0;             // (pre-defined by FIPS 180-4)
}

// sha1 block update operation
static void sha1_update(SHA1_CTX* context, const unsigned char* data, unsigned int len) {
    unsigned int i, index, part_len;                              // counters

    index = (unsigned int)((context->count[0] >> 3) & 0x3F);      // byte index
    context->count[0] += ((uint32_t)len << 3);                    // update bit count
    if (context->count[0] < ((uint32_t)len << 3))                 // overflow
        context->count[1]++;                                      // carry
    context->count[1] += ((uint32_t)len >> 29);                   // update high bits
    part_len = 64 - index;                                        // remaining space

    if (len >= part_len) {                                        // transform as needed
        memcpy(&context->buffer[index], data, part_len);          // fill buffer
        sha1_transform(context->state, context->buffer);          // transform
        for (i = part_len; i + 63 < len; i += 64)                 // transform blocks
            sha1_transform(context->state, &data[i]);             // transform
        index = 0;                                                // reset index
    } else {
        i = 0;                                                    // no transform
    }
    memcpy(&context->buffer[index], &data[i], len - i);           // buffer remaining
}

// sha1 finalization
static void sha1_final(unsigned char digest[20], SHA1_CTX* context) {
    unsigned char bits[8];                                        // bit count
    unsigned int index, pad_len;                                  // padding

    // encode bit count big-endian
    bits[0] = (unsigned char)((context->count[1] >> 24) & 0xFF);  // high byte of count[1]
    bits[1] = (unsigned char)((context->count[1] >> 16) & 0xFF);  // second byte of count[1]
    bits[2] = (unsigned char)((context->count[1] >> 8) & 0xFF);   // third byte of count[1]
    bits[3] = (unsigned char)(context->count[1] & 0xFF);          // low byte of count[1]
    bits[4] = (unsigned char)((context->count[0] >> 24) & 0xFF);  // high byte of count[0]
    bits[5] = (unsigned char)((context->count[0] >> 16) & 0xFF);  // second byte of count[0]
    bits[6] = (unsigned char)((context->count[0] >> 8) & 0xFF);   // third byte of count[0]
    bits[7] = (unsigned char)(context->count[0] & 0xFF);          // low byte of count[0]

    index = (unsigned int)((context->count[0] >> 3) & 0x3f);      // index
    pad_len = (index < 56) ? (56 - index) : (120 - index);        // padding length
    static const unsigned char padding[64] = {0x80};              // padding
    sha1_update(context, padding, pad_len);                       // pad
    sha1_update(context, bits, 8);                                // append length

    for (int i = 0; i < 5; i++) {                                 // extract state
        digest[i * 4]     = (unsigned char)((context->state[i] >> 24) & 0xFF);  // byte 0
        digest[i * 4 + 1] = (unsigned char)((context->state[i] >> 16) & 0xFF);  // byte 1
        digest[i * 4 + 2] = (unsigned char)((context->state[i] >> 8) & 0xFF);   // byte 2
        digest[i * 4 + 3] = (unsigned char)(context->state[i] & 0xFF);          // byte 3
    }
}

typedef struct {
    uint32_t state[8];        // state (a,b,c,d,e,f,g,h)
    uint32_t count[2];        // number of bits, modulo 2^64
    unsigned char buffer[64]; // input buffer
} SHA256_CTX;

// sha-256 round constants
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// sha-256 macros
#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_BSIG0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_BSIG1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SSIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SSIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

// sha-256 basic transformation
static void sha256_transform(uint32_t state[8], const unsigned char block[64]) {
    uint32_t a, b, c, d, e, f, g, h;             // working variables
    uint32_t w[64];                              // message schedule array
    int i;                                       // loop counter

    for (i = 0; i < 16; i++) {                   // copy chunk into first 16 words w[0..15]
        w[i] = ((uint32_t)block[i * 4] << 24) |  // convert 4 bytes to big-endian uint32
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }

    for (i = 16; i < 64; i++) {                  // extend the first 16 words into the remaining 48 words w[16..63]
        w[i] = SHA256_SSIG1(w[i-2]) + w[i-7] +   // σ1(w[i-2]) + w[i-7]
               SHA256_SSIG0(w[i-15]) + w[i-16];  // σ0(w[i-15]) + w[i-16]
    }

    a = state[0];  // initialize working variables with current hash value
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64; i++) {  // compression function main loop
        uint32_t t1, t2;        // temporary variables

        t1 = h + SHA256_BSIG1(e) + SHA256_CH(e, f, g) + sha256_k[i] + w[i];  // t1 = h + Σ1(e) + ch(e,f,g) + k[i] + w[i]
        t2 = SHA256_BSIG0(a) + SHA256_MAJ(a, b, c);                          // t2 = Σ0(a) + maj(a,b,c)
        h = g;        // h = g
        g = f;        // g = f
        f = e;        // f = e
        e = d + t1;   // e = d + t1
        d = c;        // d = c
        c = b;        // c = b
        b = a;        // b = a
        a = t1 + t2;  // a = t1 + t2
    }

    state[0] += a;    // add the compressed chunk to the current hash value
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

// initialize sha-256 context
static void sha256_init(SHA256_CTX* context) {
    context->count[0] = context->count[1] = 0;  // clear bit count
    context->state[0] = 0x6a09e667;             // first 32 bits of fractional part of square root of prime 2
    context->state[1] = 0xbb67ae85;             // first 32 bits of fractional part of square root of prime 3
    context->state[2] = 0x3c6ef372;             // first 32 bits of fractional part of square root of prime 5
    context->state[3] = 0xa54ff53a;             // first 32 bits of fractional part of square root of prime 7
    context->state[4] = 0x510e527f;             // first 32 bits of fractional part of square root of prime 11
    context->state[5] = 0x9b05688c;             // first 32 bits of fractional part of square root of prime 13
    context->state[6] = 0x1f83d9ab;             // first 32 bits of fractional part of square root of prime 17
    context->state[7] = 0x5be0cd19;             // first 32 bits of fractional part of square root of prime 19
}

// sha-256 block update operation
static void sha256_update(SHA256_CTX* context, const unsigned char* data, unsigned int len) {
    unsigned int i, index, part_len;                          // counters

    index = (unsigned int)((context->count[0] >> 3) & 0x3F);  // byte index
    context->count[0] += ((uint32_t)len << 3);                // update bit count
    if (context->count[0] < ((uint32_t)len << 3))             // overflow
        context->count[1]++;                                  // carry
    context->count[1] += ((uint32_t)len >> 29);               // update high bits
    part_len = 64 - index;                                    // remaining space

    if (len >= part_len) {                                    // transform as needed
        memcpy(&context->buffer[index], data, part_len);      // fill buffer
        sha256_transform(context->state, context->buffer);    // transform
        for (i = part_len; i + 63 < len; i += 64)             // transform blocks
            sha256_transform(context->state, &data[i]);       // transform
        index = 0;                                            // reset index
    } else {
        i = 0;                                                // no transform
    }
    memcpy(&context->buffer[index], &data[i], len - i);       // buffer remaining
}

// sha-256 finalization
static void sha256_final(unsigned char digest[32], SHA256_CTX* context) {
    unsigned char bits[8];                                        // bit count
    unsigned int index, pad_len;                                  // padding

    // encode bit count big-endian
    bits[0] = (unsigned char)((context->count[1] >> 24) & 0xFF);  // high byte of count[1]
    bits[1] = (unsigned char)((context->count[1] >> 16) & 0xFF);  // second byte of count[1]
    bits[2] = (unsigned char)((context->count[1] >> 8) & 0xFF);   // third byte of count[1]
    bits[3] = (unsigned char)(context->count[1] & 0xFF);          // low byte of count[1]
    bits[4] = (unsigned char)((context->count[0] >> 24) & 0xFF);  // high byte of count[0]
    bits[5] = (unsigned char)((context->count[0] >> 16) & 0xFF);  // second byte of count[0]
    bits[6] = (unsigned char)((context->count[0] >> 8) & 0xFF);   // third byte of count[0]
    bits[7] = (unsigned char)(context->count[0] & 0xFF);          // low byte of count[0]

    index = (unsigned int)((context->count[0] >> 3) & 0x3f);      // index
    pad_len = (index < 56) ? (56 - index) : (120 - index);        // padding length
    static const unsigned char padding[64] = {0x80};              // padding
    sha256_update(context, padding, pad_len);                     // pad
    sha256_update(context, bits, 8);                              // append length

    for (int i = 0; i < 8; i++) {                                 // extract state
        digest[i * 4]     = (unsigned char)((context->state[i] >> 24) & 0xFF);  // byte 0
        digest[i * 4 + 1] = (unsigned char)((context->state[i] >> 16) & 0xFF);  // byte 1
        digest[i * 4 + 2] = (unsigned char)((context->state[i] >> 8) & 0xFF);   // byte 2
        digest[i * 4 + 3] = (unsigned char)(context->state[i] & 0xFF);          // byte 3
    }
}

typedef struct {
    uint64_t state[8];          // state (A,B,C,D,E,F,G,H)
    uint64_t count[2];          // number of bits, modulo 2^128
    unsigned char buffer[128];  // input buffer
} SHA512_CTX;

// sha-384 uses same context structure as sha-512
typedef SHA512_CTX SHA384_CTX;

// sha-512 round constants
static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

// sha-512 macros
#define SHA512_ROTR(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHA512_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA512_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA512_BSIG0(x) (SHA512_ROTR(x, 28) ^ SHA512_ROTR(x, 34) ^ SHA512_ROTR(x, 39))
#define SHA512_BSIG1(x) (SHA512_ROTR(x, 14) ^ SHA512_ROTR(x, 18) ^ SHA512_ROTR(x, 41))
#define SHA512_SSIG0(x) (SHA512_ROTR(x, 1) ^ SHA512_ROTR(x, 8) ^ ((x) >> 7))
#define SHA512_SSIG1(x) (SHA512_ROTR(x, 19) ^ SHA512_ROTR(x, 61) ^ ((x) >> 6))

// sha-512 basic transformation
static void sha512_transform(uint64_t state[8], const unsigned char block[128]) {
    uint64_t a, b, c, d, e, f, g, h;             // working variables
    uint64_t w[80];                              // message schedule array
    int i;                                       // loop counter

    for (i = 0; i < 16; i++) {                   // copy chunk into first 16 words w[0..15]
        w[i] = ((uint64_t)block[i * 8] << 56) |  // convert 8 bytes to big-endian uint64
               ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) |
               ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) |
               ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8) |
               (uint64_t)block[i * 8 + 7];
    }

    for (i = 16; i < 80; i++) {                  // extend the first 16 words into the remaining 64 words w[16..79]
        w[i] = SHA512_SSIG1(w[i-2]) + w[i-7] +   // σ1(w[i-2]) + w[i-7]
               SHA512_SSIG0(w[i-15]) + w[i-16];  // σ0(w[i-15]) + w[i-16]
    }

    a = state[0];  // initialize working variables with current hash value
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 80; i++) {  // compression function main loop
        uint64_t t1, t2;        // temporary variables

        t1 = h + SHA512_BSIG1(e) + SHA512_CH(e, f, g) + sha512_k[i] + w[i];  // t1 = h + Σ1(e) + ch(e,f,g) + k[i] + w[i]
        t2 = SHA512_BSIG0(a) + SHA512_MAJ(a, b, c);                          // t2 = Σ0(a) + maj(a,b,c)
        h = g;        // h = g
        g = f;        // g = f
        f = e;        // f = e
        e = d + t1;   // e = d + t1
        d = c;        // d = c
        c = b;        // c = b
        b = a;        // b = a
        a = t1 + t2;  // a = t1 + t2
    }

    state[0] += a;    // add the compressed chunk to the current hash value
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

// initialize sha-512 context
static void sha512_init(SHA512_CTX* context) {
    context->count[0] = context->count[1] = 0;  // clear bit count
    context->state[0] = 0x6a09e667f3bcc908ULL;  // fractional part of square root of prime 2
    context->state[1] = 0xbb67ae8584caa73bULL;  // fractional part of square root of prime 3
    context->state[2] = 0x3c6ef372fe94f82bULL;  // fractional part of square root of prime 5
    context->state[3] = 0xa54ff53a5f1d36f1ULL;  // fractional part of square root of prime 7
    context->state[4] = 0x510e527fade682d1ULL;  // fractional part of square root of prime 11
    context->state[5] = 0x9b05688c2b3e6c1fULL;  // fractional part of square root of prime 13
    context->state[6] = 0x1f83d9abfb41bd6bULL;  // fractional part of square root of prime 17
    context->state[7] = 0x5be0cd19137e2179ULL;  // fractional part of square root of prime 19
}

// initialize sha-384 context (different IVs from sha-512)
static void sha384_init(SHA384_CTX* context) {
    context->count[0] = context->count[1] = 0;  // clear bit count
    context->state[0] = 0xcbbb9d5dc1059ed8ULL;  // sha-384 initial hash value A
    context->state[1] = 0x629a292a367cd507ULL;  // sha-384 initial hash value B
    context->state[2] = 0x9159015a3070dd17ULL;  // sha-384 initial hash value C
    context->state[3] = 0x152fecd8f70e5939ULL;  // sha-384 initial hash value D
    context->state[4] = 0x67332667ffc00b31ULL;  // sha-384 initial hash value E
    context->state[5] = 0x8eb44a8768581511ULL;  // sha-384 initial hash value F
    context->state[6] = 0xdb0c2e0d64f98fa7ULL;  // sha-384 initial hash value G
    context->state[7] = 0x47b5481dbefa4fa4ULL;  // sha-384 initial hash value H
}

// sha-512 block update operation (also used by sha-384)
static void sha512_update(SHA512_CTX* context, const unsigned char* data, unsigned int len) {
    unsigned int i, index, part_len;                          // counters

    index = (unsigned int)((context->count[0] >> 3) & 0x7F);  // byte index (mod 128)
    context->count[0] += ((uint64_t)len << 3);                // update bit count
    if (context->count[0] < ((uint64_t)len << 3))             // overflow
        context->count[1]++;                                  // carry
    context->count[1] += ((uint64_t)len >> 61);               // update high bits
    part_len = 128 - index;                                   // remaining space

    if (len >= part_len) {                                    // transform as needed
        memcpy(&context->buffer[index], data, part_len);      // fill buffer
        sha512_transform(context->state, context->buffer);    // transform
        for (i = part_len; i + 127 < len; i += 128)           // transform blocks
            sha512_transform(context->state, &data[i]);       // transform
        index = 0;                                            // reset index
    } else {
        i = 0;                                                // no transform
    }
    memcpy(&context->buffer[index], &data[i], len - i);       // buffer remaining
}

// sha-512 finalization
static void sha512_final(unsigned char digest[64], SHA512_CTX* context) {
    unsigned char bits[16];                                   // bit count (128 bits)
    unsigned int index, pad_len;                              // padding

    // encode 128-bit bit count big-endian
    for (int i = 0; i < 8; i++) {                             // high 64 bits
        bits[i] = (unsigned char)((context->count[1] >> (56 - i * 8)) & 0xFF);
    }
    for (int i = 0; i < 8; i++) {                             // low 64 bits
        bits[i + 8] = (unsigned char)((context->count[0] >> (56 - i * 8)) & 0xFF);
    }

    index = (unsigned int)((context->count[0] >> 3) & 0x7f);  // index (mod 128)
    pad_len = (index < 112) ? (112 - index) : (240 - index);  // padding length
    static const unsigned char padding[128] = {0x80};         // padding
    sha512_update(context, padding, pad_len);                 // pad
    sha512_update(context, bits, 16);                         // append length

    for (int i = 0; i < 8; i++) {                                       // extract state
        digest[i * 8]     = (unsigned char)((context->state[i] >> 56) & 0xFF);  // byte 0
        digest[i * 8 + 1] = (unsigned char)((context->state[i] >> 48) & 0xFF);  // byte 1
        digest[i * 8 + 2] = (unsigned char)((context->state[i] >> 40) & 0xFF);  // byte 2
        digest[i * 8 + 3] = (unsigned char)((context->state[i] >> 32) & 0xFF);  // byte 3
        digest[i * 8 + 4] = (unsigned char)((context->state[i] >> 24) & 0xFF);  // byte 4
        digest[i * 8 + 5] = (unsigned char)((context->state[i] >> 16) & 0xFF);  // byte 5
        digest[i * 8 + 6] = (unsigned char)((context->state[i] >> 8) & 0xFF);   // byte 6
        digest[i * 8 + 7] = (unsigned char)(context->state[i] & 0xFF);          // byte 7
    }
}

// sha-384 finalization (sha-512 final truncated to 48 bytes)
static void sha384_final(unsigned char digest[48], SHA384_CTX* context) {
    unsigned char full_digest[64];                            // full sha-512 digest
    sha512_final(full_digest, (SHA512_CTX*)context);          // compute full digest
    memcpy(digest, full_digest, 48);                          // truncate to 48 bytes (384 bits)
}

// converts raw digest bytes to a hex string, returns malloc'd buffer
static char* digest_to_hex(const unsigned char* digest, int digest_len) {
    static const char hex_chars[] = "0123456789abcdef";              // hex characters
    char* hex_str = (char*)malloc(digest_len * 2 + 1);               // hex buffer
    if (!hex_str) return NULL;                                       // allocation failed
    for (int i = 0; i < digest_len; i++) {                           // convert to hex
        hex_str[i * 2]     = hex_chars[(digest[i] >> 4) & 0xF];      // high nibble
        hex_str[i * 2 + 1] = hex_chars[digest[i] & 0xF];             // low nibble
    }
    hex_str[digest_len * 2] = '\0';                                  // null terminate
    return hex_str;                                                  // return hex string
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                      // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));  // intern and box as value
}

// generic string hash function
static bool hash_string(VM* vm, Value* args, Value* result,
                        void* context, int context_size,
                        hash_init_fn init, hash_update_fn update, hash_final_fn final,
                        int digest_size) {
    if (!IS_STRING(args[0])) {
        *result = MAKE_NONE();                     // invalid argument
        return true;                               // builtin handled
    }

    StringObject* input_str = AS_STRING(args[0]);  // get input string
    unsigned char* digest = (unsigned char*)alloca(digest_size);  // hash output

    init(context);                                                // initialize
    update(context, (unsigned char*)input_str->chars,             // update with data
           (unsigned int)input_str->length);                      // data length
    final(digest, context);                                       // finalize

    char* hex_str = digest_to_hex(digest, digest_size);           // convert to hex
    if (!hex_str) {                                               // allocation failed
        memset(context, 0, context_size);                         // clear sensitive data
        *result = MAKE_NONE();                                    // return none
        return true;                                              // builtin handled
    }
    *result = make_string_val(vm, hex_str);                       // return hex string
    free(hex_str);                                                // free hex buffer
    return true;                                                  // builtin handled
}

// block sizes for each hash
#define MD5_BLOCK_SIZE    64
#define SHA1_BLOCK_SIZE   64
#define SHA256_BLOCK_SIZE 64
#define SHA384_BLOCK_SIZE 128
#define SHA512_BLOCK_SIZE 128

// generic hmac implementation
static bool hmac_hash(VM* vm, Value* args, Value* result,
                      void* context, int context_size,
                      hash_init_fn init, hash_update_fn update, hash_final_fn final,
                      int block_size, int digest_size) {
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate key and message
        *result = MAKE_NONE();                         // invalid argument
        return true;                                   // builtin handled
    }

    StringObject* key_str = AS_STRING(args[0]);        // get key string
    StringObject* msg_str = AS_STRING(args[1]);        // get message string
    unsigned char* digest = (unsigned char*)alloca(digest_size);  // hmac output
    unsigned char key_block[128];                      // max block size (sha512 = 128)
    unsigned char inner_digest[64];                    // max digest size (sha512 = 64)

    memset(key_block, 0, block_size);                  // zero fill
    if (key_str->length > block_size) {                // key longer than block
        init(context);                                 // init hash
        update(context, (unsigned char*)key_str->chars, (unsigned int)key_str->length);  // hash the key
        final(key_block, context);                     // store hashed key in first digest_size bytes
    } else {
        memcpy(key_block, key_str->chars, key_str->length);   // copy key as-is
    }

    for (int i = 0; i < block_size; i++) {                    // xor key with ipad
        key_block[i] ^= 0x36;                                 // ipad = 0x36 repeated
    }
    init(context);                                            // init inner hash
    update(context, key_block, (unsigned int)block_size);     // hash (key ^ ipad)
    update(context, (unsigned char*)msg_str->chars, (unsigned int)msg_str->length);  // hash message
    final(inner_digest, context);                       // inner digest done

    for (int i = 0; i < block_size; i++) {              // xor key with opad (ipad ^ opad = 0x36 ^ 0x5c)
        key_block[i] ^= (0x36 ^ 0x5c);                  // convert from ipad to opad
    }
    init(context);                                             // init outer hash
    update(context, key_block, (unsigned int)block_size);      // hash (key ^ opad)
    update(context, inner_digest, (unsigned int)digest_size);  // hash inner digest
    final(digest, context);                                    // final hmac done

    memset(key_block, 0, sizeof(key_block));            // clear key material
    memset(inner_digest, 0, sizeof(inner_digest));      // clear inner digest
    memset(context, 0, context_size);                   // clear hash context

    char* hex_str = digest_to_hex(digest, digest_size); // convert to hex
    if (!hex_str) {                                     // allocation failed
        *result = MAKE_NONE();                          // return none
        return true;                                    // builtin handled
    }
    *result = make_string_val(vm, hex_str);             // return hex string
    free(hex_str);                                      // free hex buffer
    return true;                                        // builtin handled
}

// generic pbkdf2 implementation
static bool pbkdf2_hash(VM* vm, Value* args, Value* result,
                        void* context, int context_size,
                        hash_init_fn init, hash_update_fn update, hash_final_fn final,
                        int block_size, int digest_size) {
    (void)context_size;                                // unused, kept for uniform function signature

    if (!IS_STRING(args[0]) || !IS_STRING(args[1]) ||  // validate password and salt
        !IS_NUMBER(args[2]) || !IS_NUMBER(args[3])) {  // validate iterations and key_len
        *result = MAKE_NONE();                         // invalid argument
        return true;                                   // builtin handled
    }

    StringObject* password = AS_STRING(args[0]);       // get password
    StringObject* salt_str = AS_STRING(args[1]);       // get salt
    int iterations = (int)AS_NUMBER(args[2]);          // get iteration count
    int key_len = (int)AS_NUMBER(args[3]);             // get desired key length in bytes

    if (iterations < 1 || key_len < 1) {               // validate positive values
        *result = MAKE_NONE();                         // invalid
        return true;                                   // builtin handled
    }

    int block_count = (key_len + digest_size - 1) / digest_size;  // ceiling division

    if (block_count > 1024) {                          // sanity limit to avoid huge allocations
        *result = MAKE_NONE();                         // too large
        return true;                                   // builtin handled
    }

    unsigned char* output = (unsigned char*)malloc(key_len);  // allocate output key buffer
    if (!output) {                                            // allocation failed
        *result = MAKE_NONE();
        return true;
    }

    // step 1: pre-hash password if longer than block size (RFC 2104 key preparation)
    unsigned char key_block[128];                             // prepared key (max block size = 128)
    memset(key_block, 0, block_size);                         // zero fill
    if (password->length > block_size) {                      // key longer than block size
        init(context);                                        // init hash
        update(context, (unsigned char*)password->chars, (unsigned int)password->length);  // hash the key
        final(key_block, context);                            // store hashed key
    } else {
        memcpy(key_block, password->chars, password->length);  // copy key as-is
    }

    unsigned char u[64];                                       // temp buffer for each U iteration
    unsigned char block_result[64];                            // XOR accumulator for current block

    // allocate salt_block = salt || block_index (4 bytes for BE32)
    unsigned char* salt_block = (unsigned char*)malloc(salt_str->length + 4);  // salt || i
    if (!salt_block) {                                         // allocation failed
        free(output);
        *result = MAKE_NONE();
        return true;
    }

    memcpy(salt_block, salt_str->chars, salt_str->length);     // copy salt prefix

    // step 2: for each block T_i = F(password, salt, iterations, i)
    for (int block = 0; block < block_count; block++) {  // iterate over blocks
        int block_idx = block + 1;                       // 1-based block index

        salt_block[salt_str->length + 0] = (unsigned char)((block_idx >> 24) & 0xFF);  // high byte
        salt_block[salt_str->length + 1] = (unsigned char)((block_idx >> 16) & 0xFF);  // second byte
        salt_block[salt_str->length + 2] = (unsigned char)((block_idx >> 8) & 0xFF);   // third byte
        salt_block[salt_str->length + 3] = (unsigned char)(block_idx & 0xFF);          // low byte

        unsigned char ipad[128];                // inner padding buffer
        memcpy(ipad, key_block, block_size);    // copy prepared key
        for (int j = 0; j < block_size; j++) {  // XOR with ipad (0x36)
            ipad[j] ^= 0x36;                    // ipad byte
        }
        init(context);                                                      // init inner hash
        update(context, ipad, (unsigned int)block_size);                    // hash (key ^ ipad)
        update(context, salt_block, (unsigned int)(salt_str->length + 4));  // hash (salt || i)
        final(u, context);                                                  // inner digest in u

        unsigned char opad[128];                              // outer padding buffer
        memcpy(opad, key_block, block_size);                  // copy prepared key
        for (int j = 0; j < block_size; j++) {                // XOR with opad (0x5c)
            opad[j] ^= 0x5c;                                  // opad byte
        }
        init(context);                                        // init outer hash
        update(context, opad, (unsigned int)block_size);      // hash (key ^ opad)
        update(context, u, (unsigned int)digest_size);        // hash inner digest
        final(u, context);                                    // U1 done (stored in u)
        memcpy(block_result, u, digest_size);                 // initialize accumulator

        for (int iter = 1; iter < iterations; iter++) {       // for each additional iteration
            memcpy(ipad, key_block, block_size);              // copy prepared key
            for (int j = 0; j < block_size; j++) {            // XOR with ipad
                ipad[j] ^= 0x36;                              // ipad byte
            }
            init(context);                                    // init inner hash
            update(context, ipad, (unsigned int)block_size);  // hash (key ^ ipad)
            update(context, u, (unsigned int)digest_size);    // hash previous u
            final(u, context);                                // inner digest in u

            // outer hash: H((key ^ opad) || u)
            memcpy(opad, key_block, block_size);             // copy prepared key
            for (int j = 0; j < block_size; j++) {           // XOR with opad
                opad[j] ^= 0x5c;                             // opad byte
            }
            init(context);                                    // init outer hash
            update(context, opad, (unsigned int)block_size);  // hash (key ^ opad)
            update(context, u, (unsigned int)digest_size);    // hash inner digest
            final(u, context);                                // U_i done (stored in u)

            // block_result ^= U_i (XOR accumulation)
            for (int j = 0; j < digest_size; j++) {            // XOR each byte
                block_result[j] ^= u[j];                       // accumulate
            }
        }

        // copy block_result to output
        int copy_len = digest_size;                                    // full digest by default
        if (block == block_count - 1 && key_len % digest_size != 0) {  // last block may be partial
            copy_len = key_len % digest_size;                          // partial copy
        }
        memcpy(output + block * digest_size, block_result, copy_len);  // copy to output
    }

    // clear sensitive data from memory
    memset(salt_block, 0, salt_str->length + 4);     // clear salt block
    free(salt_block);                                // free salt block
    memset(key_block, 0, sizeof(key_block));         // clear prepared key
    memset(u, 0, sizeof(u));                         // clear temp buffer
    memset(block_result, 0, sizeof(block_result));   // clear accumulator

    char* hex_str = digest_to_hex(output, key_len);  // convert to hex string
    free(output);                                    // free output buffer
    if (!hex_str) {                                  // allocation failed
        *result = MAKE_NONE();                       // return none
        return true;                                 // builtin handled
    }
    *result = make_string_val(vm, hex_str);          // return hex string
    free(hex_str);                                   // free hex buffer
    return true;                                     // builtin handled
}

// block and key size constants for aes
#define AES_BLOCK_SIZE 16
#define AES128_KEY_SIZE 16
#define AES192_KEY_SIZE 24
#define AES256_KEY_SIZE 32

// aes number of rounds
#define AES128_ROUNDS 10
#define AES192_ROUNDS 12
#define AES256_ROUNDS 14

// aes expanded key sizes
#define AES128_EXPANDED_KEY_SIZE 176  // 16 * (10 + 1)
#define AES192_EXPANDED_KEY_SIZE 208  // 16 * (12 + 1)
#define AES256_EXPANDED_KEY_SIZE 240  // 16 * (14 + 1)

// aes s-box lookup table
static const unsigned char aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// aes inverse s-box lookup table
static const unsigned char aes_inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// aes round constants for key expansion
static const unsigned char aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// galois field multiplication by 2 (xtime)
static unsigned char aes_xtime(unsigned char x) {
    return (unsigned char)((x << 1) ^ (((x >> 7) & 1) * 0x1b));  // shift left, reduce modulo poly if overflow
}

// galois field multiplication by 3
static unsigned char aes_mult3(unsigned char x) {
    return aes_xtime(x) ^ x;  // 3*x = 2*x xor x
}

// galois field multiplication in gf(2^8)
static unsigned char aes_gmul(unsigned char a, unsigned char b) {
    unsigned char p = 0;                   // product
    unsigned char hi_bit_set;              // high bit flag
    for (int i = 0; i < 8; i++) {          // iterate over 8 bits
        if (b & 1) p ^= a;                 // if low bit of b is 1, add a to product
        hi_bit_set = a & 0x80;             // check high bit of a
        a <<= 1;                           // shift a left
        if (hi_bit_set) a ^= 0x1b;         // reduce modulo irreducible polynomial
        b >>= 1;                           // shift b right
    }
    return p;                              // return product
}

// aes-128 key expansion from 16-byte key to 176-byte round keys
static void aes128_key_expansion(const unsigned char* key, unsigned char* round_keys) {
    for (int i = 0; i < 16; i++) {         // copy original key
        round_keys[i] = key[i];            // to first 16 bytes
    }
    
    int bytes_generated = 16;              // count of generated bytes
    int rcon_iteration = 1;                // round constant index
    unsigned char temp[4];                 // temporary word
    
    while (bytes_generated < 176) {        // 11 * 16 bytes for aes-128
        for (int i = 0; i < 4; i++) {      // copy last word
            temp[i] = round_keys[bytes_generated - 4 + i];  // to temp
        }
        
        if (bytes_generated % 16 == 0) {   // every 16 bytes
            unsigned char k = temp[0];     // rotate word left
            temp[0] = temp[1];             // by one byte
            temp[1] = temp[2];             // rotword
            temp[2] = temp[3];             // operation
            temp[3] = k;                   // complete rotation
            
            for (int i = 0; i < 4; i++) {     // substitute each byte
                temp[i] = aes_sbox[temp[i]];  // using s-box
            }
            
            temp[0] ^= aes_rcon[rcon_iteration++];  // xor with round constant
        }
        
        for (int i = 0; i < 4; i++) {       // generate new word
            round_keys[bytes_generated] = round_keys[bytes_generated - 16] ^ temp[i];  // xor with word 16 bytes back
            bytes_generated++;              // increment counter
        }
    }
}

// aes-192 key expansion from 24-byte key to 208-byte round keys
static void aes192_key_expansion(const unsigned char* key, unsigned char* round_keys) {
    for (int i = 0; i < 24; i++) {         // copy original key
        round_keys[i] = key[i];            // to first 24 bytes
    }
    
    int bytes_generated = 24;              // count of generated bytes
    int rcon_iteration = 1;                // round constant index
    unsigned char temp[4];                 // temporary word
    
    while (bytes_generated < 208) {        // 13 * 16 bytes for aes-192
        for (int i = 0; i < 4; i++) {      // copy last word
            temp[i] = round_keys[bytes_generated - 4 + i];  // to temp
        }
        
        if (bytes_generated % 24 == 0) {   // every 24 bytes
            unsigned char k = temp[0];     // rotate word left
            temp[0] = temp[1];             // by one byte
            temp[1] = temp[2];             // rotword
            temp[2] = temp[3];             // operation
            temp[3] = k;                   // complete rotation
            
            for (int i = 0; i < 4; i++) {     // substitute each byte
                temp[i] = aes_sbox[temp[i]];  // using s-box
            }
            
            temp[0] ^= aes_rcon[rcon_iteration++];  // xor with round constant
        }
        
        for (int i = 0; i < 4; i++) {       // generate new word
            round_keys[bytes_generated] = round_keys[bytes_generated - 24] ^ temp[i];  // xor with word 24 bytes back
            bytes_generated++;              // increment counter
        }
    }
}

// aes-256 key expansion from 32-byte key to 240-byte round keys
static void aes256_key_expansion(const unsigned char* key, unsigned char* round_keys) {
    for (int i = 0; i < 32; i++) {         // copy original key
        round_keys[i] = key[i];            // to first 32 bytes
    }
    
    int bytes_generated = 32;              // count of generated bytes
    int rcon_iteration = 1;                // round constant index
    unsigned char temp[4];                 // temporary word
    
    while (bytes_generated < 240) {        // 15 * 16 bytes for aes-256
        for (int i = 0; i < 4; i++) {      // copy last word
            temp[i] = round_keys[bytes_generated - 4 + i];  // to temp
        }
        
        if (bytes_generated % 32 == 0) {   // every 32 bytes
            unsigned char k = temp[0];     // rotate word left
            temp[0] = temp[1];             // by one byte
            temp[1] = temp[2];             // rotword
            temp[2] = temp[3];             // operation
            temp[3] = k;                   // complete rotation
            
            for (int i = 0; i < 4; i++) {     // substitute each byte
                temp[i] = aes_sbox[temp[i]];  // using s-box
            }
            
            temp[0] ^= aes_rcon[rcon_iteration++];  // xor with round constant
        } else if (bytes_generated % 32 == 16) {    // extra s-box step for aes-256
            for (int i = 0; i < 4; i++) {           // substitute each byte
                temp[i] = aes_sbox[temp[i]];        // using s-box
            }
        }
        
        for (int i = 0; i < 4; i++) {       // generate new word
            round_keys[bytes_generated] = round_keys[bytes_generated - 32] ^ temp[i];  // xor with word 32 bytes back
            bytes_generated++;              // increment counter
        }
    }
}

// add round key to state by xor
static void aes_add_round_key(unsigned char* state, const unsigned char* round_key) {
    for (int i = 0; i < 16; i++) {          // iterate over 16 bytes
        state[i] ^= round_key[i];           // xor state with round key
    }
}

// substitute bytes using s-box
static void aes_sub_bytes(unsigned char* state) {
    for (int i = 0; i < 16; i++) {          // iterate over state
        state[i] = aes_sbox[state[i]];      // substitute each byte
    }
}

// inverse substitute bytes using inverse s-box
static void aes_inv_sub_bytes(unsigned char* state) {
    for (int i = 0; i < 16; i++) {          // iterate over state
        state[i] = aes_inv_sbox[state[i]];  // inverse substitute each byte
    }
}

// shift rows step of aes
static void aes_shift_rows(unsigned char* state) {
    unsigned char temp;     // temporary storage
    
    temp = state[1];        // save first byte
    state[1] = state[5];    // move byte
    state[5] = state[9];    // move byte
    state[9] = state[13];   // move byte
    state[13] = temp;       // restore saved byte
    
    temp = state[2];        // save and swap
    state[2] = state[10];   // bytes at distance 2
    state[10] = temp;       // complete first swap
    temp = state[6];        // save and swap
    state[6] = state[14];   // second pair
    state[14] = temp;       // complete second swap
    
    temp = state[15];       // shift right by 1
    state[15] = state[11];  // move byte
    state[11] = state[7];   // move byte
    state[7] = state[3];    // move byte
    state[3] = temp;        // restore saved byte
}

// inverse shift rows step of aes
static void aes_inv_shift_rows(unsigned char* state) {
    unsigned char temp;     // temporary storage
    
    temp = state[13];       // save last byte
    state[13] = state[9];   // move byte
    state[9] = state[5];    // move byte
    state[5] = state[1];    // move byte
    state[1] = temp;        // restore saved byte
    
    temp = state[2];        // save and swap
    state[2] = state[10];   // bytes at distance 2
    state[10] = temp;       // complete first swap
    temp = state[6];        // save and swap
    state[6] = state[14];   // second pair
    state[14] = temp;       // complete second swap
    
    temp = state[3];        // shift left by 1
    state[3] = state[7];    // move byte
    state[7] = state[11];   // move byte
    state[11] = state[15];  // move byte
    state[15] = temp;       // restore saved byte
}

// mix columns step using galois field arithmetic
static void aes_mix_columns(unsigned char* state) {
    for (int i = 0; i < 4; i++) {           // process each column
        int col = i * 4;                    // column start index
        unsigned char a = state[col];       // byte 0 of column
        unsigned char b = state[col + 1];   // byte 1 of column
        unsigned char c = state[col + 2];   // byte 2 of column
        unsigned char d = state[col + 3];   // byte 3 of column
        
        state[col]     = aes_xtime(a) ^ aes_mult3(b) ^ c ^ d;  // 2*a + 3*b + c + d
        state[col + 1] = a ^ aes_xtime(b) ^ aes_mult3(c) ^ d;  // a + 2*b + 3*c + d
        state[col + 2] = a ^ b ^ aes_xtime(c) ^ aes_mult3(d);  // a + b + 2*c + 3*d
        state[col + 3] = aes_mult3(a) ^ b ^ c ^ aes_xtime(d);  // 3*a + b + c + 2*d
    }
}

// inverse mix columns step
static void aes_inv_mix_columns(unsigned char* state) {
    for (int i = 0; i < 4; i++) {           // process each column
        int col = i * 4;                    // column start index
        unsigned char a = state[col];       // byte 0 of column
        unsigned char b = state[col + 1];   // byte 1 of column
        unsigned char c = state[col + 2];   // byte 2 of column
        unsigned char d = state[col + 3];   // byte 3 of column
        
        state[col]     = aes_gmul(a, 0x0e) ^ aes_gmul(b, 0x0b) ^ aes_gmul(c, 0x0d) ^ aes_gmul(d, 0x09);  // 14*a + 11*b + 13*c + 9*d
        state[col + 1] = aes_gmul(a, 0x09) ^ aes_gmul(b, 0x0e) ^ aes_gmul(c, 0x0b) ^ aes_gmul(d, 0x0d);  // 9*a + 14*b + 11*c + 13*d
        state[col + 2] = aes_gmul(a, 0x0d) ^ aes_gmul(b, 0x09) ^ aes_gmul(c, 0x0e) ^ aes_gmul(d, 0x0b);  // 13*a + 9*b + 14*c + 11*d
        state[col + 3] = aes_gmul(a, 0x0b) ^ aes_gmul(b, 0x0d) ^ aes_gmul(c, 0x09) ^ aes_gmul(d, 0x0e);  // 11*a + 13*b + 9*c + 14*d
    }
}

// aes encryption of a single 16-byte block (generic, takes round count)
static void aes_encrypt_block_generic(const unsigned char* input, const unsigned char* round_keys, unsigned char* output, int rounds) {
    unsigned char state[16];               // internal state
    
    for (int i = 0; i < 16; i++) {         // copy input to state
        state[i] = input[i];               // byte by byte
    }
    
    aes_add_round_key(state, round_keys);  // initial round key addition
    
    for (int round = 1; round < rounds; round++) {  // main rounds
        aes_sub_bytes(state);              // substitute bytes
        aes_shift_rows(state);             // shift rows
        aes_mix_columns(state);            // mix columns
        aes_add_round_key(state, round_keys + round * 16);  // add round key
    }
    
    // final round without mixcolumns
    aes_sub_bytes(state);                  // substitute bytes
    aes_shift_rows(state);                 // shift rows
    aes_add_round_key(state, round_keys + rounds * 16);  // add last round key
    
    for (int i = 0; i < 16; i++) {         // copy state to output
        output[i] = state[i];              // byte by byte
    }
}

// aes decryption of a single 16-byte block (generic, takes round count)
static void aes_decrypt_block_generic(const unsigned char* input, const unsigned char* round_keys, unsigned char* output, int rounds) {
    unsigned char state[16];               // internal state
    
    for (int i = 0; i < 16; i++) {         // copy input to state
        state[i] = input[i];               // byte by byte
    }
    
    aes_add_round_key(state, round_keys + rounds * 16);  // add round key
    aes_inv_shift_rows(state);                           // inverse shift rows
    aes_inv_sub_bytes(state);                            // inverse substitute bytes
    
    for (int round = rounds - 1; round > 0; round--) {  // rounds-1 down to 1
        aes_add_round_key(state, round_keys + round * 16);  // add round key
        aes_inv_mix_columns(state);                         // inverse mix columns
        aes_inv_shift_rows(state);                          // inverse shift rows
        aes_inv_sub_bytes(state);                           // inverse substitute bytes
    }
    
    aes_add_round_key(state, round_keys);  // add initial round key
    
    for (int i = 0; i < 16; i++) {         // copy state to output
        output[i] = state[i];              // byte by byte
    }
}

// aes-128 encryption of a single 16-byte block
static void aes128_encrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_encrypt_block_generic(input, round_keys, output, AES128_ROUNDS);  // 10 rounds for aes-128
}

// aes-128 decryption of a single 16-byte block
static void aes128_decrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_decrypt_block_generic(input, round_keys, output, AES128_ROUNDS);  // 10 rounds for aes-128
}

// aes-192 encryption of a single 16-byte block
static void aes192_encrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_encrypt_block_generic(input, round_keys, output, AES192_ROUNDS);  // 12 rounds for aes-192
}

// aes-192 decryption of a single 16-byte block
static void aes192_decrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_decrypt_block_generic(input, round_keys, output, AES192_ROUNDS);  // 12 rounds for aes-192
}

// aes-256 encryption of a single 16-byte block
static void aes256_encrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_encrypt_block_generic(input, round_keys, output, AES256_ROUNDS);  // 14 rounds for aes-256
}

// aes-256 decryption of a single 16-byte block
static void aes256_decrypt_block(const unsigned char* input, const unsigned char* round_keys, unsigned char* output) {
    aes_decrypt_block_generic(input, round_keys, output, AES256_ROUNDS);  // 14 rounds for aes-256
}

// generic aes-cbc encryption: key and plaintext required, iv optional (default zero)
static bool aes_cbc_encrypt_generic(VM* vm, Value* args, Value* result,
                                     int key_size, int expanded_key_size,
                                     void (*key_expansion)(const unsigned char*, unsigned char*),
                                     void (*encrypt_block)(const unsigned char*, const unsigned char*, unsigned char*)) {
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {   // validate arguments
        *result = MAKE_NONE();                          // invalid
        return true;                                    // builtin handled
    }
    
    unsigned char key[32];                              // aes key (max 32 for aes-256)
    unsigned char iv[16];                               // initialization vector
    memset(iv, 0, 16);                                  // default iv = all zeros
    
    if (hex_to_bytes(AS_STRING(args[0])->chars, key, key_size) != key_size) {  // parse key from hex
        *result = MAKE_NONE();                          // invalid key
        return true;                                    // builtin handled
    }
    
    if (IS_STRING(args[2])) {                           // iv provided
        if (hex_to_bytes(AS_STRING(args[2])->chars, iv, 16) != 16) {  // parse iv from hex
            *result = MAKE_NONE();                      // invalid iv
            return true;                                // builtin handled
        }
    }
    
    StringObject* plaintext = AS_STRING(args[1]);       // get plaintext string
    int plaintext_len = plaintext->length;              // plaintext length
    
    int pad_byte = 16 - (plaintext_len % 16);           // padding byte value
    if (pad_byte == 0) pad_byte = 16;                   // full block padding if aligned
    int padded_len = plaintext_len + pad_byte;          // padded length
    
    unsigned char* padded = (unsigned char*)malloc(padded_len);  // allocate padded buffer
    if (!padded) {                                      // allocation failed
        *result = MAKE_NONE();                          // return none
        return true;                                    // builtin handled
    }
    
    memcpy(padded, plaintext->chars, plaintext_len);     // copy plaintext
    memset(padded + plaintext_len, pad_byte, pad_byte);  // add padding bytes
    
    unsigned char* round_keys = (unsigned char*)malloc(expanded_key_size);  // expanded key schedule
    if (!round_keys) {                                  // allocation failed
        free(padded);                                   // free padded buffer        *result = MAKE_NONE();                          // return none
        return true;                                    // builtin handled
    }
    key_expansion(key, round_keys);                     // expand key
    
    // cbc mode encryption
    unsigned char prev_block[16];                       // previous ciphertext block
    memcpy(prev_block, iv, 16);                         // start with iv
    
    for (int i = 0; i < padded_len; i += 16) {          // process each block
        for (int j = 0; j < 16; j++) {                  // xor with previous block
            padded[i + j] ^= prev_block[j];             // cbc xor step
        }
        
        encrypt_block(padded + i, round_keys, prev_block);  // encrypt block
        memcpy(padded + i, prev_block, 16);             // copy ciphertext back
    }
    
    // convert ciphertext to hex string
    char* hex_str = (char*)malloc(padded_len * 2 + 1);  // hex string buffer
    if (!hex_str) {                                     // allocation failed
        free(padded);                                   // free buffers
        free(round_keys);                               // free round keys
        memset(key, 0, sizeof(key));                    // clear key
        *result = MAKE_NONE();                          // return none
        return true;                                    // builtin handled
    }
    
    bytes_to_hex(padded, padded_len, hex_str);          // convert to hex
    *result = make_string_val(vm, hex_str);             // return hex string
    
    // cleanup sensitive data
    free(padded);                                       // free padded buffer
    free(hex_str);                                      // free hex string
    free(round_keys);                                   // free round keys
    memset(key, 0, sizeof(key));                        // clear key
    memset(iv, 0, 16);                                  // clear iv
    
    return true;                                        // builtin handled
}

// generic aes-cbc decryption: key and ciphertext required, iv optional (default zero)
static bool aes_cbc_decrypt_generic(VM* vm, Value* args, Value* result,
                                     int key_size, int expanded_key_size,
                                     void (*key_expansion)(const unsigned char*, unsigned char*),
                                     void (*decrypt_block)(const unsigned char*, const unsigned char*, unsigned char*)) {
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate arguments
        *result = MAKE_NONE();                         // invalid
        return true;                                   // builtin handled
    }
    
    unsigned char key[32];                             // aes key (max 32 for aes-256)
    unsigned char iv[16];                              // initialization vector
    memset(iv, 0, 16);                                 // default iv = all zeros
    
    if (hex_to_bytes(AS_STRING(args[0])->chars, key, key_size) != key_size) {  // parse key from hex
        *result = MAKE_NONE();                         // invalid key
        return true;                                   // builtin handled
    }
    
    if (IS_STRING(args[2])) {                          // iv provided
        if (hex_to_bytes(AS_STRING(args[2])->chars, iv, 16) != 16) {  // parse iv from hex
            *result = MAKE_NONE();                     // invalid iv
            return true;                               // builtin handled
        }
    }
    
    StringObject* ciphertext_hex = AS_STRING(args[1]); // get ciphertext hex string
    int hex_len = ciphertext_hex->length;              // hex string length
    
    if (hex_len % 2 != 0 || hex_len < 32) {            // must be even and at least one block
        *result = MAKE_NONE();                         // invalid input
        return true;                                   // builtin handled
    }
    
    int ciphertext_len = hex_len / 2;                  // binary ciphertext length
    unsigned char* ciphertext = (unsigned char*)malloc(ciphertext_len);  // allocate ciphertext buffer
    if (!ciphertext) {                                 // allocation failed
        *result = MAKE_NONE();                         // return none
        return true;                                   // builtin handled
    }
    
    if (hex_to_bytes(ciphertext_hex->chars, ciphertext, ciphertext_len) != ciphertext_len) {  // parse hex
        free(ciphertext);                              // free buffer
        *result = MAKE_NONE();                         // invalid hex
        return true;                                   // builtin handled
    }
    
    unsigned char* round_keys = (unsigned char*)malloc(expanded_key_size);  // expanded key schedule
    if (!round_keys) {                                 // allocation failed
        free(ciphertext);                              // free buffer
        *result = MAKE_NONE();                         // return none
        return true;                                   // builtin handled
    }
    key_expansion(key, round_keys);                    // expand key
    
    unsigned char prev_block[16];                      // previous ciphertext block
    memcpy(prev_block, iv, 16);                        // start with iv
    unsigned char current_block[16];                   // current block storage
    
    for (int i = 0; i < ciphertext_len; i += 16) {     // process each block
        memcpy(current_block, ciphertext + i, 16);     // save current ciphertext
        
        decrypt_block(ciphertext + i, round_keys, ciphertext + i);  // decrypt block
        
        for (int j = 0; j < 16; j++) {                 // xor with previous block
            ciphertext[i + j] ^= prev_block[j];        // cbc xor step
        }
        
        memcpy(prev_block, current_block, 16);         // update previous block
    }
    
    int pad_len = ciphertext[ciphertext_len - 1];      // get padding length from last byte
    if (pad_len < 1 || pad_len > 16) {                 // invalid padding
        free(ciphertext);                              // free buffer
        free(round_keys);                              // free round keys
        memset(key, 0, sizeof(key));                   // clear key
        *result = MAKE_NONE();                         // return none
        return true;                                   // builtin handled
    }
    
    for (int i = 0; i < pad_len; i++) {                // check each padding byte
        if (ciphertext[ciphertext_len - 1 - i] != pad_len) {  // invalid padding byte
            free(ciphertext);                          // free buffer
            free(round_keys);                          // free round keys
            memset(key, 0, sizeof(key));               // clear key
            *result = MAKE_NONE();                     // return none
            return true;                               // builtin handled
        }
    }
    
    int plaintext_len = ciphertext_len - pad_len;      // actual plaintext length
    
    char* plaintext_str = (char*)malloc(plaintext_len + 1);  // allocate string buffer
    if (!plaintext_str) {                              // allocation failed
        free(ciphertext);                              // free buffer
        free(round_keys);                              // free round keys
        memset(key, 0, sizeof(key));                   // clear key
        *result = MAKE_NONE();                         // return none
        return true;                                   // builtin handled
    }
    
    memcpy(plaintext_str, ciphertext, plaintext_len);  // copy plaintext
    plaintext_str[plaintext_len] = '\0';               // null terminate
    
    *result = make_string_val(vm, plaintext_str);      // return plaintext string
    
    free(ciphertext);                                  // free ciphertext buffer
    free(plaintext_str);                               // free string buffer
    free(round_keys);                                  // free round keys
    memset(key, 0, sizeof(key));                       // clear key
    memset(iv, 0, 16);                                 // clear iv
    
    return true;                                       // builtin handled
}

// aes-128-cbc encryption: key and plaintext required, iv optional (default zero)
static bool aes128_encrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_encrypt_generic(vm, args, result,
                                    AES128_KEY_SIZE, AES128_EXPANDED_KEY_SIZE,
                                    aes128_key_expansion, aes128_encrypt_block);
}

// aes-128-cbc decryption: key and ciphertext required, iv optional (default zero)
static bool aes128_decrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_decrypt_generic(vm, args, result,
                                    AES128_KEY_SIZE, AES128_EXPANDED_KEY_SIZE,
                                    aes128_key_expansion, aes128_decrypt_block);
}

// aes-192-cbc encryption: key and plaintext required, iv optional (default zero)
static bool aes192_encrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_encrypt_generic(vm, args, result,
                                    AES192_KEY_SIZE, AES192_EXPANDED_KEY_SIZE,
                                    aes192_key_expansion, aes192_encrypt_block);
}

// aes-192-cbc decryption: key and ciphertext required, iv optional (default zero)
static bool aes192_decrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_decrypt_generic(vm, args, result,
                                    AES192_KEY_SIZE, AES192_EXPANDED_KEY_SIZE,
                                    aes192_key_expansion, aes192_decrypt_block);
}

// aes-256-cbc encryption: key and plaintext required, iv optional (default zero)
static bool aes256_encrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_encrypt_generic(vm, args, result,
                                    AES256_KEY_SIZE, AES256_EXPANDED_KEY_SIZE,
                                    aes256_key_expansion, aes256_encrypt_block);
}

// aes-256-cbc decryption: key and ciphertext required, iv optional (default zero)
static bool aes256_decrypt(VM* vm, Value* args, Value* result) {
    return aes_cbc_decrypt_generic(vm, args, result,
                                    AES256_KEY_SIZE, AES256_EXPANDED_KEY_SIZE,
                                    aes256_key_expansion, aes256_decrypt_block);
}

// dispatcher for crypto built-in functions
bool crypto_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (arg_count < 1) {
        *result = MAKE_NONE();
        return true;
    }

    if (strcmp(name, "crypto.md5") == 0) {               // md5 hash of string
        MD5_CTX context;
        return hash_string(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)md5_init, (hash_update_fn)md5_update,
                          (hash_final_fn)md5_final, 16);
    }

    if (strcmp(name, "crypto.sha1") == 0) {              // sha1 hash of string
        SHA1_CTX context;
        return hash_string(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha1_init, (hash_update_fn)sha1_update,
                          (hash_final_fn)sha1_final, 20);
    }

    if (strcmp(name, "crypto.sha256") == 0) {            // sha-256 hash of string
        SHA256_CTX context;
        return hash_string(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha256_init, (hash_update_fn)sha256_update,
                          (hash_final_fn)sha256_final, 32);
    }

    if (strcmp(name, "crypto.sha384") == 0) {            // sha-384 hash of string
        SHA384_CTX context;
        return hash_string(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha384_init, (hash_update_fn)sha512_update,
                          (hash_final_fn)sha384_final, 48);
    }

    if (strcmp(name, "crypto.sha512") == 0) {            // sha-512 hash of string
        SHA512_CTX context;
        return hash_string(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha512_init, (hash_update_fn)sha512_update,
                          (hash_final_fn)sha512_final, 64);
    }

    if (strcmp(name, "crypto.hmac_md5") == 0) {          // hmac-md5(key, msg)
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        MD5_CTX context;
        return hmac_hash(vm, args, result, &context, sizeof(context),
                        (hash_init_fn)md5_init, (hash_update_fn)md5_update,
                        (hash_final_fn)md5_final, MD5_BLOCK_SIZE, 16);
    }

    if (strcmp(name, "crypto.hmac_sha1") == 0) {         // hmac-sha1(key, msg)
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        SHA1_CTX context;
        return hmac_hash(vm, args, result, &context, sizeof(context),
                        (hash_init_fn)sha1_init, (hash_update_fn)sha1_update,
                        (hash_final_fn)sha1_final, SHA1_BLOCK_SIZE, 20);
    }

    if (strcmp(name, "crypto.hmac_sha256") == 0) {       // hmac-sha256(key, msg)
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        SHA256_CTX context;
        return hmac_hash(vm, args, result, &context, sizeof(context),
                        (hash_init_fn)sha256_init, (hash_update_fn)sha256_update,
                        (hash_final_fn)sha256_final, SHA256_BLOCK_SIZE, 32);
    }

    if (strcmp(name, "crypto.hmac_sha384") == 0) {       // hmac-sha384(key, msg)
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        SHA384_CTX context;
        return hmac_hash(vm, args, result, &context, sizeof(context),
                        (hash_init_fn)sha384_init, (hash_update_fn)sha512_update,
                        (hash_final_fn)sha384_final, SHA384_BLOCK_SIZE, 48);
    }

    if (strcmp(name, "crypto.hmac_sha512") == 0) {       // hmac-sha512(key, msg)
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        SHA512_CTX context;
        return hmac_hash(vm, args, result, &context, sizeof(context),
                        (hash_init_fn)sha512_init, (hash_update_fn)sha512_update,
                        (hash_final_fn)sha512_final, SHA512_BLOCK_SIZE, 64);
    }

    if (strcmp(name, "crypto.pbkdf2_md5") == 0) {        // pbkdf2-hmac-md5
        if (arg_count < 4) { *result = MAKE_NONE(); return true; }
        MD5_CTX context;
        return pbkdf2_hash(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)md5_init, (hash_update_fn)md5_update,
                          (hash_final_fn)md5_final, MD5_BLOCK_SIZE, 16);
    }

    if (strcmp(name, "crypto.pbkdf2_sha1") == 0) {       // pbkdf2-hmac-sha1
        if (arg_count < 4) { *result = MAKE_NONE(); return true; }
        SHA1_CTX context;
        return pbkdf2_hash(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha1_init, (hash_update_fn)sha1_update,
                          (hash_final_fn)sha1_final, SHA1_BLOCK_SIZE, 20);
    }

    if (strcmp(name, "crypto.pbkdf2_sha256") == 0) {     // pbkdf2-hmac-sha256
        if (arg_count < 4) { *result = MAKE_NONE(); return true; }
        SHA256_CTX context;
        return pbkdf2_hash(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha256_init, (hash_update_fn)sha256_update,
                          (hash_final_fn)sha256_final, SHA256_BLOCK_SIZE, 32);
    }

    if (strcmp(name, "crypto.pbkdf2_sha384") == 0) {     // pbkdf2-hmac-sha384
        if (arg_count < 4) { *result = MAKE_NONE(); return true; }
        SHA384_CTX context;
        return pbkdf2_hash(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha384_init, (hash_update_fn)sha512_update,
                          (hash_final_fn)sha384_final, SHA384_BLOCK_SIZE, 48);
    }

    if (strcmp(name, "crypto.pbkdf2_sha512") == 0) {     // pbkdf2-hmac-sha512
        if (arg_count < 4) { *result = MAKE_NONE(); return true; }
        SHA512_CTX context;
        return pbkdf2_hash(vm, args, result, &context, sizeof(context),
                          (hash_init_fn)sha512_init, (hash_update_fn)sha512_update,
                          (hash_final_fn)sha512_final, SHA512_BLOCK_SIZE, 64);
    }

    if (strcmp(name, "crypto.aes128_encrypt") == 0) {    // aes-128-cbc encrypt(key, plaintext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes128_encrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.aes128_decrypt") == 0) {    // aes-128-cbc decrypt(key, ciphertext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes128_decrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.aes192_encrypt") == 0) {    // aes-192-cbc encrypt(key, plaintext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes192_encrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.aes192_decrypt") == 0) {    // aes-192-cbc decrypt(key, ciphertext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes192_decrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.aes256_encrypt") == 0) {    // aes-256-cbc encrypt(key, plaintext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes256_encrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.aes256_decrypt") == 0) {    // aes-256-cbc decrypt(key, ciphertext, [iv])
        if (arg_count < 2) { *result = MAKE_NONE(); return true; }
        return aes256_decrypt(vm, args, result);
    }

    if (strcmp(name, "crypto.token_hex") == 0) {                     // secure hex token
        if (arg_count != 1 || !IS_NUMBER(args[0])) {                 // need exactly 1 number arg
            *result = MAKE_NONE();
            return true;
        }
        int nbytes = (int)AS_NUMBER(args[0]);                        // number of bytes
        if (nbytes <= 0) {                                           // invalid size
            *result = MAKE_NONE();
            return true;
        }
        unsigned char* buffer = (unsigned char*)malloc(nbytes);      // allocate buffer
        if (!buffer) {                                               // allocation failed
            *result = MAKE_NONE();
            return true;
        }
        get_secure_bytes(buffer, nbytes);                            // fill with secure bytes
        char* hex_str = (char*)malloc(nbytes * 2 + 1);               // allocate hex string
        if (!hex_str) {                                              // allocation failed
            free(buffer);                                            // free buffer
            *result = MAKE_NONE();
            return true;
        }
        bytes_to_hex(buffer, nbytes, hex_str);                       // convert to hex
        free(buffer);                                                // free buffer
        *result = make_string_val(vm, hex_str);                      // return hex string
        free(hex_str);                                               // free hex string
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "crypto.secure_randint") == 0) {     // secure random integer
        if (arg_count != 1 || !IS_NUMBER(args[0])) {      // validate
            *result = MAKE_NONE();
            return true;
        }
        int n = (int)AS_NUMBER(args[0]);                  // modulo
        if (n <= 0) {                                     // invalid
            *result = MAKE_NONE();
            return true;
        }
        unsigned char rb;                                  // random byte
        get_secure_bytes(&rb, 1);                          // get secure byte
        *result = MAKE_NUMBER((double)(rb % n));           // reduce modulo n
        return true;                                       // builtin handled
    }

    if (strcmp(name, "crypto.compare_digest") == 0) {      // constant-time compare
        if (arg_count != 2) {                              // need 2 args
            *result = MAKE_BOOL(false);
            return true;
        }
        if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {  // validate strings
            *result = MAKE_BOOL(false);
            return true;
        }
        StringObject* sa = AS_STRING(args[0]);             // string a
        StringObject* sb = AS_STRING(args[1]);             // string b
        bool match = constant_time_compare(sa->chars, sb->chars, sa->length, sb->length);  // compare
        *result = MAKE_BOOL(match);                        // return result
        return true;  // builtin handled
    }

    return false;  // not a recognized builtin
}