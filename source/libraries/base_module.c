// source/libraries/base_module.c
// Implementation of Base Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "base_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

// standard base64 character set with padding
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

// url-safe base64 character set (uses - and _ instead of + and /)
static const char b64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";

// hexadecimal character set for base16 encoding
static const char b16_chars[] = "0123456789ABCDEF";

// standard base32 character set (RFC 4648)
static const char b32_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567=";

// extended hex base32 character set (RFC 4648)
static const char b32hex_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV=";

// base62 character set (0-9, A-Z, a-z)
static const char b62_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// ascii85 character set (Adobe version) - corrected to proper ASCII85 alphabet
static const char b85_chars[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstu";

// generic function to encode binary data with variable bit grouping
static void base_generic_encode(const unsigned char* data, int len, char* out, 
                                 const char* alphabet, int bits_per_char, int pad_to_multiple) {
    int i = 0, j = 0;                                                        // input and output indices
    uint64_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining in buffer
    uint32_t mask = (1U << bits_per_char) - 1;                              // mask for extracting bits
    
    while (i < len) {                                                        // iterate over input bytes
        buffer = (buffer << 8) | data[i++];                                  // add byte to buffer
        bits_left += 8;                                                      // increment bits
        while (bits_left >= bits_per_char) {                                 // extract bit chunks
            int char_idx = (buffer >> (bits_left - bits_per_char)) & mask;   // get character index
            out[j++] = alphabet[char_idx];                                   // append encoded char
            bits_left -= bits_per_char;                                      // remove processed bits
        }
    }
    
    if (bits_left > 0) {                                                     // leftover bits
        int char_idx = (buffer << (bits_per_char - bits_left)) & mask;       // pad with zeros
        out[j++] = alphabet[char_idx];                                       // append last char
    }
    
    while (j % pad_to_multiple != 0) {                                       // add padding
        out[j++] = '=';                                                      // padding char
    }
    out[j] = '\0';                                                           // null terminate
}

// generic function to decode base encoded data with variable bit grouping
static bool base_generic_decode(const char* str, unsigned char* out, int* out_len,
                                 int (*decode_char)(char), int bits_per_char, int pad_to_multiple,
                                 int* valid_padding_bits, int num_valid_padding_bits) {
    int len = strlen(str);                                                   // input length
    if (len == 0) {                                                          // empty input
        *out_len = 0;                                                        // empty output
        return true;
    }
    
    uint64_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    *out_len = 0;                                                            // output length
    int padding = 0;                                                         // padding count
    int non_padding_len = 0;                                                 // length without padding
    
    for (int i = 0; i < len; i++) {                                          // iterate over input
        if (str[i] == '=') {                                                 // padding
            padding++;                                                       // count padding
            continue;                                                        // skip
        }
        if (padding > 0) return false;                                      // padding in middle
        
        int val = decode_char(str[i]);                                       // decode char
        if (val < 0) return false;                                           // invalid
        
        buffer = (buffer << bits_per_char) | val;                            // add to buffer
        bits_left += bits_per_char;                                          // increment bits
        
        if (bits_left >= 8) {                                                // have enough for byte
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8)); // extract byte
            bits_left -= 8;                                                  // remove processed bits
        }
        non_padding_len++;                                                   // count non-padding chars
    }
    
    // validate padding
    if (padding > 0) {
        // check that total length is multiple of pad_to_multiple
        if (len % pad_to_multiple != 0) return false;
        
        // check that padding count is valid
        if (padding >= pad_to_multiple) return false;
        
        // check that remaining bits match expected padding
        bool valid_bits = false;
        for (int i = 0; i < num_valid_padding_bits; i++) {
            if (bits_left == valid_padding_bits[i]) {
                valid_bits = true;
                break;
            }
        }
        if (!valid_bits) return false;
    } else {
        // no padding, check if length is valid
        if (len % pad_to_multiple != 0) return false;
    }
    
    return true;                                                             // success
}

// encodes binary data to base64 with proper padding
static void base64_encode(const unsigned char* data, int len, char* out) {
    base_generic_encode(data, len, out, b64_chars, 6, 4);
}

// decodes a single base64 character, returns -1 on invalid
static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';                                // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;                           // lowercase
    if (c >= '0' && c <= '9') return c - '0' + 52;                           // digits
    if (c == '+') return 62;                                                 // plus
    if (c == '/') return 63;                                                 // slash
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a base64 string into binary data
static bool base64_decode(const char* str, unsigned char* out, int* out_len) {
    // valid padding bits: 1 '=' = 2 bits, 2 '=' = 4 bits
    int valid_padding_bits[] = {2, 4};
    return base_generic_decode(str, out, out_len, base64_decode_char, 6, 4,
                               valid_padding_bits, 2);
}

// encodes binary data to url-safe base64 without padding
static void base64url_encode(const unsigned char* data, int len, char* out) {
    int i = 0, j = 0;                                                        // input and output indices
    uint64_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    uint32_t mask = 0x3F;                                                    // mask for 6 bits
    
    while (i < len) {                                                        // iterate over input
        buffer = (buffer << 8) | data[i++];                                  // add byte
        bits_left += 8;                                                      // increment bits
        while (bits_left >= 6) {                                             // extract 6-bit chunks
            int char_idx = (buffer >> (bits_left - 6)) & mask;              // get index
            out[j++] = b64url_chars[char_idx];                              // append url-safe char
            bits_left -= 6;                                                  // remove processed bits
        }
    }
    
    if (bits_left > 0) {                                                     // leftover bits
        int char_idx = (buffer << (6 - bits_left)) & mask;                  // pad with zeros
        out[j++] = b64url_chars[char_idx];                                  // append last char
    }
    
    out[j] = '\0';                                                           // null terminate (no padding)
}

// decodes a url-safe base64 character
static int base64url_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';                                // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;                           // lowercase
    if (c >= '0' && c <= '9') return c - '0' + 52;                           // digits
    if (c == '-') return 62;                                                 // dash (url-safe)
    if (c == '_') return 63;                                                 // underscore (url-safe)
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a url-safe base64 string
static bool base64url_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                                   // input length
    if (len == 0) {                                                          // empty input
        *out_len = 0;                                                        // empty output
        return true;
    }
    
    uint64_t buffer = 0;                                                     // bit buffer
    int bits_left = 0;                                                       // bits remaining
    *out_len = 0;                                                            // output length
    
    for (int i = 0; i < len; i++) {                                          // iterate over input
        if (str[i] == '=') break;                                            // padding stops decoding
        int val = base64url_decode_char(str[i]);                             // decode char
        if (val < 0) return false;                                           // invalid
        
        buffer = (buffer << 6) | val;                                        // add to buffer
        bits_left += 6;                                                      // increment bits
        
        if (bits_left >= 8) {                                                // have enough for byte
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8)); // extract byte
            bits_left -= 8;                                                  // remove processed bits
        }
    }
    return true;                                                             // success
}

// encodes binary data to base16 (hexadecimal)
static void base16_encode(const unsigned char* data, int len, char* out) {
    for (int i = 0; i < len; i++) {                                          // iterate over input bytes
        out[i * 2] = b16_chars[(data[i] >> 4) & 0x0F];                       // high nibble
        out[i * 2 + 1] = b16_chars[data[i] & 0x0F];                          // low nibble
    }
    out[len * 2] = '\0';                                                     // null terminate
}

// decodes a single hexadecimal character
static int base16_decode_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';                                // digits
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;                           // uppercase
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;                           // lowercase
    return -1;                                                               // invalid
}

// decodes a base16 (hexadecimal) string into binary data
static bool base16_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                                   // input length
    if (len == 0) {                                                          // empty input
        *out_len = 0;                                                        // empty output
        return true;
    }
    
    if (len % 2 != 0) return false;                                          // must be even length
    
    *out_len = 0;                                                            // output length
    
    for (int i = 0; i < len; i += 2) {                                       // iterate over pairs
        int high = base16_decode_char(str[i]);                               // decode high nibble
        int low = base16_decode_char(str[i + 1]);                            // decode low nibble
        
        if (high < 0 || low < 0) return false;                              // invalid character
        
        out[(*out_len)++] = (unsigned char)((high << 4) | low);              // combine nibbles
    }
    
    return true;                                                             // success
}

// encodes binary data to base32 with padding
static void base32_encode(const unsigned char* data, int len, char* out) {
    base_generic_encode(data, len, out, b32_chars, 5, 8);
}

// decodes a single base32 character
static int base32_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';                                // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a';                                // lowercase (case-insensitive)
    if (c >= '2' && c <= '7') return c - '2' + 26;                           // digits 2-7
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a base32 string into binary data
static bool base32_decode(const char* str, unsigned char* out, int* out_len) {
    // valid padding bits for base32: 1 '=' = 3 bits, 3 '=' = 1 bit, 4 '=' = 4 bits, 6 '=' = 2 bits
    int valid_padding_bits[] = {3, 1, 4, 2};
    return base_generic_decode(str, out, out_len, base32_decode_char, 5, 8,
                               valid_padding_bits, 4);
}

// encodes binary data to base32hex with padding
static void base32hex_encode(const unsigned char* data, int len, char* out) {
    base_generic_encode(data, len, out, b32hex_chars, 5, 8);
}

// decodes a single base32hex character
static int base32hex_decode_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';                                // digits
    if (c >= 'A' && c <= 'V') return c - 'A' + 10;                           // uppercase A-V
    if (c >= 'a' && c <= 'v') return c - 'a' + 10;                           // lowercase a-v
    if (c == '=') return 0;                                                  // padding
    return -1;                                                               // invalid
}

// decodes a base32hex string into binary data
static bool base32hex_decode(const char* str, unsigned char* out, int* out_len) {
    // valid padding bits for base32hex: 1 '=' = 3 bits, 3 '=' = 1 bit, 4 '=' = 4 bits, 6 '=' = 2 bits
    int valid_padding_bits[] = {3, 1, 4, 2};
    return base_generic_decode(str, out, out_len, base32hex_decode_char, 5, 8,
                               valid_padding_bits, 4);
}

// encodes binary data to base62 (0-9, A-Z, a-z)
static void base62_encode(const unsigned char* data, int len, char* out) {
    if (len == 0) {                                              // empty input
        out[0] = '\0';                                           // empty output
        return;
    }
    
    int leading_zeros = 0;                                       // count leading zeros
    while (leading_zeros < len && data[leading_zeros] == 0) {
        leading_zeros++;
    }
    
    if (leading_zeros == len) {                                  // if all zeros
        for (int i = 0; i < len; i++) {
            out[i] = '0';                                        // output that many '0' characters
        }
        out[len] = '\0';
        return;
    }
    
    int work_len = len - leading_zeros;                          // create working buffer (skip leading zeros)
    unsigned char* work = (unsigned char*)malloc(work_len);
    if (!work) {                                                 // allocation failed
        out[0] = '\0';
        return;
    }
    memcpy(work, data + leading_zeros, work_len);
    
    int max_out = work_len * 2 + 1;                              // allocate output buffer
    char* reversed = (char*)malloc(max_out);
    if (!reversed) {                                             // allocation failed
        free(work);
        out[0] = '\0';
        return;
    }
    
    int out_pos = 0;
    
    while (work_len > 0) {                                       // repeated division by 62
        int remainder = 0;                                       // current remainder
        int new_len = 0;                                         // new length
        
        for (int i = 0; i < work_len; i++) {                    // divide by 62
            unsigned int dividend = remainder * 256 + work[i];  // current dividend
            unsigned int quotient = dividend / 62;              // quotient
            remainder = dividend % 62;                           // remainder
            
            if (new_len > 0 || quotient > 0) {                  // skip leading zeros in quotient
                work[new_len++] = quotient;
            }
        }
        
        reversed[out_pos++] = b62_chars[remainder];              // store remainder
        work_len = new_len;                                      // update length
    }
    
    for (int i = 0; i < leading_zeros; i++) {                   // add leading zeros
        reversed[out_pos++] = '0';
    }
    
    for (int i = 0; i < out_pos; i++) {                         // reverse into output
        out[i] = reversed[out_pos - 1 - i];
    }
    out[out_pos] = '\0';                                         // null terminate
    
    free(work);                                                  // free memory
    free(reversed);
}

// decodes a single base62 character
static int base62_decode_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';                   // digits
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;              // uppercase
    if (c >= 'a' && c <= 'z') return c - 'a' + 36;              // lowercase
    return -1;                                                   // invalid
}

// decodes a base62 string into binary data
static bool base62_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                       // input length
    if (len == 0) {                                              // empty input
        *out_len = 0;                                            // empty output
        return true;
    }
    
    int leading_zeros = 0;                                       // count leading zeros
    while (leading_zeros < len && str[leading_zeros] == '0') {
        leading_zeros++;
    }
    
    if (leading_zeros == len) {                                  // if all zeros
        memset(out, 0, len);                                     // output that many zero bytes
        *out_len = len;
        return true;
    }
    
    int num_digits = len - leading_zeros;                        // use a simple approach: convert base62 string to a big integer
    
    size_t max_bytes = (size_t)num_digits * 2 + 1;              // allocate big integer (stored as array of bytes, little-endian)
    if (max_bytes > (size_t)INT_MAX) return false;              // sanity check
    
    unsigned char* bigint = (unsigned char*)calloc(max_bytes, 1);
    if (!bigint) return false;                                   // allocation failed
    int bigint_len = 0;
    
    for (int i = leading_zeros; i < len; i++) {                 // process each base62 digit
        int val = base62_decode_char(str[i]);                   // decode char
        if (val < 0) {                                           // invalid char
            free(bigint);
            return false;
        }
        
        int carry = 0;                                           // multiply bigint by 62
        for (int j = 0; j < bigint_len; j++) {
            int product = bigint[j] * 62 + carry;               // product
            bigint[j] = product & 0xFF;                          // store low byte
            carry = product >> 8;                                // carry high byte
        }
        while (carry > 0) {                                      // handle remaining carry
            if (bigint_len >= (int)max_bytes) {                  // buffer overflow protection
                free(bigint);
                return false;
            }
            bigint[bigint_len++] = carry & 0xFF;                // store byte
            carry >>= 8;                                         // next byte
        }
        
        carry = val;                                             // add val to bigint
        for (int j = 0; j < bigint_len && carry > 0; j++) {
            int sum = bigint[j] + carry;                         // sum
            bigint[j] = sum & 0xFF;                              // store low byte
            carry = sum >> 8;                                    // carry high byte
        }
        while (carry > 0) {                                      // handle remaining carry
            if (bigint_len >= (int)max_bytes) {                  // buffer overflow protection
                free(bigint);
                return false;
            }
            bigint[bigint_len++] = carry & 0xFF;                // store byte
            carry >>= 8;                                         // next byte
        }
    }
    
    *out_len = leading_zeros + bigint_len;                       // output leading zeros
    memset(out, 0, leading_zeros);
    
    for (int i = 0; i < bigint_len; i++) {                      // reverse bigint (little-endian to big-endian)
        out[leading_zeros + i] = bigint[bigint_len - 1 - i];
    }
    
    free(bigint);                                                // free memory
    return true;                                                 // success
}

// encodes binary data to ascii85 (Adobe version)
static void base85_encode(const unsigned char* data, int len, char* out) {
    int i = 0;                                                   // input index
    int j = 0;                                                   // output index
    
    while (i < len) {                                            // process input
        int remaining = len - i;                                 // remaining bytes
        
        if (remaining >= 4) {                                    // full group of 4 bytes
            uint32_t value = ((uint32_t)data[i] << 24) |        // 4 bytes to 32-bit
                             ((uint32_t)data[i+1] << 16) |
                             ((uint32_t)data[i+2] << 8) |
                             (uint32_t)data[i+3];
            
            if (value == 0) {                                    // special case: all zeros
                out[j++] = 'z';                                  // encode as 'z'
            } else {
                uint32_t temp = value;                           // calculate 5 base85 digits
                int digit[5];
                for (int k = 4; k >= 0; k--) {
                    digit[k] = temp % 85;                        // extract digit
                    temp /= 85;
                }
                for (int k = 0; k < 5; k++) {
                    out[j++] = b85_chars[digit[k]];             // output char
                }
            }
            i += 4;                                              // advance input
        } else {
            uint32_t value = 0;                                  // partial group
            for (int k = 0; k < remaining; k++) {               // build value
                value = (value << 8) | data[i + k];
            }
            
            for (int k = 0; k < 4 - remaining; k++) {           // pad with zeros to make 4 bytes
                value <<= 8;
            }
            
            uint32_t temp = value;                               // calculate base85 digits
            int digit[5];
            for (int k = 4; k >= 0; k--) {
                digit[k] = temp % 85;                            // extract digit
                temp /= 85;
            }
            
            for (int k = 0; k < remaining + 1; k++) {           // output remaining + 1 characters
                out[j++] = b85_chars[digit[k]];
            }
            i += remaining;                                      // advance input
        }
    }
    
    out[j] = '\0';                                               // null terminate
}

// decodes a single ascii85 character
static int base85_decode_char(char c) {
    if (c >= '!' && c <= 'u') return c - '!';                   // printable chars
    if (c == 'z') return -2;                                     // special marker
    return -1;                                                   // invalid
}

// decodes an ascii85 string into binary data
static bool base85_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);                                       // input length
    if (len == 0) {                                              // empty input
        *out_len = 0;                                            // empty output
        return true;
    }
    
    *out_len = 0;                                                // output length
    int i = 0;                                                   // input index
    
    while (i < len) {                                            // process input
        if (str[i] == 'z') {                                     // handle zero marker
            out[(*out_len)++] = 0;                               // output four zeros
            out[(*out_len)++] = 0;
            out[(*out_len)++] = 0;
            out[(*out_len)++] = 0;
            i++;                                                 // advance input
            continue;
        }
        
        int chars[5];                                            // collect up to 5 characters
        int count = 0;                                           // character count
        
        while (count < 5 && i < len && str[i] != 'z') {         // collect chars
            int val = base85_decode_char(str[i]);               // decode char
            if (val < 0) return false;                           // invalid char
            chars[count++] = val;                                // store value
            i++;                                                 // advance input
        }
        
        if (count == 0) break;                                   // no chars
        
        for (int k = count; k < 5; k++) {                       // pad with 'u' (84) for incomplete groups
            chars[k] = 84;
        }
        
        uint32_t value = 0;                                      // calculate value
        for (int k = 0; k < 5; k++) {
            value = value * 85 + chars[k];
        }
        
        for (int k = 0; k < count - 1; k++) {                   // output bytes (count - 1 for partial groups)
            out[(*out_len)++] = (value >> (24 - k * 8)) & 0xFF;
        }
    }
    
    return true;                                                 // success
}

// generic dispatch helper for encode functions
static bool dispatch_encode(VM* vm, int arg_count, Value* args, Value* result,
                            void (*encode_func)(const unsigned char*, int, char*),
                            int (*calc_out_size)(int)) {
    if (arg_count < 1 || !IS_STRING(args[0])) {                             // validate string
        *result = MAKE_NONE();                                               // invalid
        return true;                                                         // builtin handled
    }
    
    StringObject* input_str = AS_STRING(args[0]);                            // input string
    int input_len = input_str->length;                                       // input length
    int out_size = calc_out_size(input_len);                                 // calculate output size
    char* out = (char*)malloc(out_size);                                     // allocate output
    if (!out) { *result = MAKE_NONE(); return true; }                        // allocation failed
    
    encode_func((const unsigned char*)input_str->chars, input_len, out);    // encode
    *result = MAKE_STRING(string_intern(&vm->intern_table, out, strlen(out))); // intern result
    free(out);                                                               // free output
    return true;                                                             // builtin handled
}

// generic dispatch helper for decode functions
static bool dispatch_decode(VM* vm, int arg_count, Value* args, Value* result,
                            bool (*decode_func)(const char*, unsigned char*, int*)) {
    if (arg_count < 1 || !IS_STRING(args[0])) {                             // validate string
        *result = MAKE_NONE();                                               // invalid
        return true;                                                         // builtin handled
    }
    
    StringObject* input_str = AS_STRING(args[0]);                            // input string
    int input_len = input_str->length;                                       // input length
    unsigned char* out = (unsigned char*)malloc(input_len * 2 + 1);        // allocate output
    if (!out) { *result = MAKE_NONE(); return true; }                       // allocation failed
    
    int out_len = 0;                                                         // output length
    if (decode_func(input_str->chars, out, &out_len)) {                     // decode
        out[out_len] = '\0';                                                 // null terminate
        *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len)); // intern result
    } else {
        *result = MAKE_NONE();                                               // decode failed
    }
    free(out);                                                               // free output
    return true;                                                             // builtin handled
}

// size calculation functions
static int calc_size_base64(int input_len) {
    return ((input_len + 2) / 3) * 4 + 1;
}

static int calc_size_base32(int input_len) {
    return ((input_len + 4) / 5) * 8 + 1;
}

static int calc_size_base16(int input_len) {
    return input_len * 2 + 1;
}

static int calc_size_base62(int input_len) {
    // base62 can be up to ~1.37x the input size
    return input_len * 2 + 1;
}

static int calc_size_base85(int input_len) {
    // ascii85 encodes 4 bytes to 5 chars (or less)
    return ((input_len + 3) / 4) * 5 + 1;
}

// main dispatcher for base module built-in functions
bool base_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "base.encode_64") == 0) {                               // base64 encode
        return dispatch_encode(vm, arg_count, args, result, base64_encode, calc_size_base64);
    }
    
    if (strcmp(name, "base.decode_64") == 0) {                               // base64 decode
        return dispatch_decode(vm, arg_count, args, result, base64_decode);
    }

    if (strcmp(name, "base.encode_64url") == 0) {                            // base64url encode
        return dispatch_encode(vm, arg_count, args, result, base64url_encode, calc_size_base64);
    }
    
    if (strcmp(name, "base.decode_64url") == 0) {                            // base64url decode
        return dispatch_decode(vm, arg_count, args, result, base64url_decode);
    }

    if (strcmp(name, "base.encode_16") == 0) {                               // base16 (hex) encode
        return dispatch_encode(vm, arg_count, args, result, base16_encode, calc_size_base16);
    }
    
    if (strcmp(name, "base.decode_16") == 0) {                               // base16 (hex) decode
        return dispatch_decode(vm, arg_count, args, result, base16_decode);
    }

    if (strcmp(name, "base.encode_32") == 0) {                               // base32 encode
        return dispatch_encode(vm, arg_count, args, result, base32_encode, calc_size_base32);
    }
    
    if (strcmp(name, "base.decode_32") == 0) {                               // base32 decode
        return dispatch_decode(vm, arg_count, args, result, base32_decode);
    }

    if (strcmp(name, "base.encode_32hex") == 0) {                            // base32hex encode
        return dispatch_encode(vm, arg_count, args, result, base32hex_encode, calc_size_base32);
    }
    
    if (strcmp(name, "base.decode_32hex") == 0) {                            // base32hex decode
        return dispatch_decode(vm, arg_count, args, result, base32hex_decode);
    }

    if (strcmp(name, "base.encode_62") == 0) {                               // base62 encode
        return dispatch_encode(vm, arg_count, args, result, base62_encode, calc_size_base62);
    }
    
    if (strcmp(name, "base.decode_62") == 0) {                               // base62 decode
        return dispatch_decode(vm, arg_count, args, result, base62_decode);
    }

    if (strcmp(name, "base.encode_85") == 0) {                               // ascii85 encode
        return dispatch_encode(vm, arg_count, args, result, base85_encode, calc_size_base85);
    }
    
    if (strcmp(name, "base.decode_85") == 0) {                               // ascii85 decode
        return dispatch_decode(vm, arg_count, args, result, base85_decode);
    }

    return false;                                                            // not a recognized builtin
}