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
        // Check that total length is multiple of pad_to_multiple
        if (len % pad_to_multiple != 0) return false;
        
        // Check that padding count is valid
        if (padding >= pad_to_multiple) return false;
        
        // Check that remaining bits match expected padding
        bool valid_bits = false;
        for (int i = 0; i < num_valid_padding_bits; i++) {
            if (bits_left == valid_padding_bits[i]) {
                valid_bits = true;
                break;
            }
        }
        if (!valid_bits) return false;
    } else {
        // No padding, check if length is valid
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
    // Valid padding bits: 1 '=' = 2 bits, 2 '=' = 4 bits
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
    // Valid padding bits for base32: 1 '=' = 3 bits, 3 '=' = 1 bit, 4 '=' = 4 bits, 6 '=' = 2 bits
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
    // Valid padding bits for base32hex: 1 '=' = 3 bits, 3 '=' = 1 bit, 4 '=' = 4 bits, 6 '=' = 2 bits
    int valid_padding_bits[] = {3, 1, 4, 2};
    return base_generic_decode(str, out, out_len, base32hex_decode_char, 5, 8,
                               valid_padding_bits, 4);
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
    unsigned char* out = (unsigned char*)malloc(input_len + 1);             // allocate output
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

    return false;                                                            // not a recognized builtin
}