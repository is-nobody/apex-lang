#include "base_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// ========== Helper: Base16 (Hex) ==========

static const char hex_chars[] = "0123456789ABCDEF";

static void base16_encode(const unsigned char* data, int len, char* out) {
    for (int i = 0; i < len; i++) {
        out[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static int base16_decode_char(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool base16_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);
    if (len % 2 != 0) return false;
    
    *out_len = len / 2;
    for (int i = 0; i < *out_len; i++) {
        int high = base16_decode_char(str[i * 2]);
        int low = base16_decode_char(str[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        out[i] = (unsigned char)((high << 4) | low);
    }
    return true;
}

// ========== Helper: Base32 (RFC 4648) ==========

static const char b32_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567=";

static void base32_encode(const unsigned char* data, int len, char* out) {
    int i = 0, idx = 0;
    uint64_t buffer = 0;
    int bits_left = 0;

    while (i < len) {
        buffer = (buffer << 8) | data[i++];
        bits_left += 8;

        while (bits_left >= 5) {
            int char_idx = (buffer >> (bits_left - 5)) & 0x1F;
            out[idx++] = b32_chars[char_idx];
            bits_left -= 5;
        }
    }

    if (bits_left > 0) {
        int char_idx = (buffer << (5 - bits_left)) & 0x1F;
        out[idx++] = b32_chars[char_idx];
    }

    // Padding
    while (idx % 8 != 0) {
        out[idx++] = '=';
    }
    out[idx] = '\0';
}

static int base32_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    if (c == '=') return 0; // Padding
    return -1;
}

static bool base32_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);
    if (len == 0) {
        *out_len = 0;
        return true;
    }
    
    uint64_t buffer = 0;
    int bits_left = 0;
    *out_len = 0;
    
    for (int i = 0; i < len; i++) {
        if (str[i] == '=') break;
        
        int val = base32_decode_char(str[i]);
        if (val < 0) return false;
        
        buffer = (buffer << 5) | val;
        bits_left += 5;
        
        if (bits_left >= 8) {
            out[(*out_len)++] = (unsigned char)(buffer >> (bits_left - 8));
            bits_left -= 8;
        }
    }
    return true;
}

// ========== Helper: Ascii85 ==========

static void a85_encode(const unsigned char* data, int len, char* out) {
    int i = 0, j = 0;
    while (i < len) {
        uint32_t block = 0;
        int bytes_read = 0;
        
        for (int k = 0; k < 4; k++) {
            block <<= 8;
            if (i < len) {
                block |= data[i++];
                bytes_read++;
            } else {
                block |= 0; // Pad with zero
            }
        }
        
        if (bytes_read == 4 && block == 0) {
            out[j++] = 'z';
        } else {
            char chars[5];
            for (int k = 4; k >= 0; k--) {
                chars[k] = (block % 85) + 33;
                block /= 85;
            }
            for (int k = 0; k < bytes_read + 1; k++) {
                out[j++] = chars[k];
            }
        }
    }
    out[j] = '\0';
}

static bool a85_decode(const char* str, unsigned char* out, int* out_len) {
    int len = strlen(str);
    int i = 0, j = 0;
    *out_len = 0;
    
    while (i < len) {
        if (str[i] == 'z') {
            out[j++] = 0; out[j++] = 0; out[j++] = 0; out[j++] = 0;
            i++;
        } else {
            uint32_t block = 0;
            int chars_read = 0;
            
            for (int k = 0; k < 5; k++) {
                if (i >= len) break;
                char c = str[i++];
                if (c < 33 || c > 117) return false; // Invalid range
                block = block * 85 + (c - 33);
                chars_read++;
            }
            
            if (chars_read > 0) {
                for (int k = 3; k >= 0; k--) {
                    if (k < chars_read - 1) {
                        out[j++] = (block >> (k * 8)) & 0xFF;
                    }
                }
            }
        }
    }
    *out_len = j;
    return true;
}

// ========== String Builder (Local) ==========
typedef struct {
    char* buffer;
    int length;
    int capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb, int initial_capacity) {
    sb->capacity = initial_capacity > 16 ? initial_capacity : 16;
    sb->buffer = (char*)malloc(sb->capacity);
    if (!sb->buffer) { sb->length = 0; return; }
    sb->length = 0;
    sb->buffer[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str, int len) {
    if (!sb->buffer) return;
    if (sb->length + len + 1 > sb->capacity) {
        int new_cap = (sb->length + len + 1) * 2;
        char* new_buf = (char*)realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
}

static void sb_append_char(StringBuilder* sb, char c) {
    if (!sb->buffer) return;
    if (sb->length + 2 > sb->capacity) {
        int new_cap = (sb->length + 2) * 2;
        char* new_buf = (char*)realloc(sb->buffer, new_cap);
        if (!new_buf) return;
        sb->buffer = new_buf;
        sb->capacity = new_cap;
    }
    sb->buffer[sb->length++] = c;
    sb->buffer[sb->length] = '\0';
}

static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);
    sb->buffer = NULL;
    sb->length = 0;
}

// ========== API Implementation ==========

bool base_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (arg_count < 1 || args[0].type != VAL_STRING) {
        *result = vm_make_bool(false);
        return true;
    }
    
    const char* input = args[0].string->chars;
    int input_len = args[0].string->length;
    
    // --- Write Functions (Encode) ---
    if (strcmp(name, "base.b16write") == 0 || strcmp(name, "base.b16encode") == 0) {
        char* out = (char*)malloc(input_len * 2 + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        base16_encode((const unsigned char*)input, input_len, out);
        *result = vm_make_string(out);
        free(out);
        return true;
    }
    
    if (strcmp(name, "base.b32write") == 0 || strcmp(name, "base.b32encode") == 0) {
        // Max size is ceil(len/5)*8 + 1
        int out_size = ((input_len + 4) / 5) * 8 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = vm_make_bool(false); return true; }
        base32_encode((const unsigned char*)input, input_len, out);
        *result = vm_make_string(out);
        free(out);
        return true;
    }
    
    if (strcmp(name, "base.a85write") == 0 || strcmp(name, "base.a85encode") == 0) {
        // Max size is ceil(len/4)*5 + 1
        int out_size = ((input_len + 3) / 4) * 5 + 1;
        char* out = (char*)malloc(out_size);
        if (!out) { *result = vm_make_bool(false); return true; }
        a85_encode((const unsigned char*)input, input_len, out);
        *result = vm_make_string(out);
        free(out);
        return true;
    }

    // --- Read Functions (Decode) ---
    if (strcmp(name, "base.b16read") == 0 || strcmp(name, "base.b16decode") == 0) {
        unsigned char* out = (unsigned char*)malloc(input_len / 2 + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        int out_len = 0;
        if (base16_decode(input, out, &out_len)) {
            out[out_len] = '\0';
            *result = vm_make_string((char*)out);
        } else {
            *result = vm_make_bool(false);
        }
        free(out);
        return true;
    }
    
    if (strcmp(name, "base.b32read") == 0 || strcmp(name, "base.b32decode") == 0) {
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        int out_len = 0;
        if (base32_decode(input, out, &out_len)) {
            out[out_len] = '\0';
            *result = vm_make_string((char*)out);
        } else {
            *result = vm_make_bool(false);
        }
        free(out);
        return true;
    }
    
    if (strcmp(name, "base.a85read") == 0 || strcmp(name, "base.a85decode") == 0) {
        unsigned char* out = (unsigned char*)malloc(input_len + 1);
        if (!out) { *result = vm_make_bool(false); return true; }
        int out_len = 0;
        if (a85_decode(input, out, &out_len)) {
            out[out_len] = '\0';
            *result = vm_make_string((char*)out);
        } else {
            *result = vm_make_bool(false);
        }
        free(out);
        return true;
    }
    
    // --- Generic write/read (Identity) ---
    if (strcmp(name, "base.write") == 0) {
        *result = vm_copy_value(args[0]);
        return true;
    }
    if (strcmp(name, "base.read") == 0) {
        *result = vm_copy_value(args[0]);
        return true;
    }
    
    // --- URL Encode/Decode ---
    if (strcmp(name, "base.url_write") == 0 || strcmp(name, "base.url_encode") == 0) {
        StringBuilder sb;
        sb_init(&sb, input_len * 3);
        for (int i = 0; i < input_len; i++) {
            char c = input[i];
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                sb_append_char(&sb, c);
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                sb_append(&sb, buf, 3);
            }
        }
        *result = vm_make_string(sb.buffer);
        sb_free(&sb);
        return true;
    }
    
    if (strcmp(name, "base.url_read") == 0 || strcmp(name, "base.url_decode") == 0) {
        StringBuilder sb;
        sb_init(&sb, input_len);
        for (int i = 0; i < input_len; i++) {
            if (input[i] == '%' && i + 2 < input_len) {
                char hex[3] = { input[i+1], input[i+2], '\0' };
                char val = (char)strtol(hex, NULL, 16);
                sb_append_char(&sb, val);
                i += 2;
            } else if (input[i] == '+') {
                sb_append_char(&sb, ' ');
            } else {
                sb_append_char(&sb, input[i]);
            }
        }
        *result = vm_make_string(sb.buffer);
        sb_free(&sb);
        return true;
    }

    return false;
}