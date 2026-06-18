#include "secrets_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper to generate secure random bytes
static void get_secure_bytes(unsigned char* buffer, size_t length) {
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t read = fread(buffer, 1, length, f);
        fclose(f);
        if (read == length) return;
    }

    // Fallback to stdlib rand()
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (unsigned char)(rand() % 256);
    }
}

// Helper: Convert bytes to hex string
static void bytes_to_hex(const unsigned char* bytes, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex_chars[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex_chars[bytes[i] & 0xF];
    }
    out[len * 2] = '\0';
}

// Helper: Base64 URL-safe alphabet
static const char base64url_alphabet[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void bytes_to_base64url(const unsigned char* bytes, size_t len, char* out) {
    size_t i = 0;
    size_t out_idx = 0;
    
    while (i < len) {
        unsigned int b0 = bytes[i++];
        unsigned int b1 = (i < len) ? bytes[i++] : 0;
        unsigned int b2 = (i < len) ? bytes[i++] : 0;
        
        unsigned int triple = (b0 << 16) | (b1 << 8) | b2;
        
        out[out_idx++] = base64url_alphabet[(triple >> 18) & 0x3F];
        out[out_idx++] = base64url_alphabet[(triple >> 12) & 0x3F];
        
        if (i - 1 < len) {
             out[out_idx++] = base64url_alphabet[(triple >> 6) & 0x3F];
        }
        if (i < len) {
             out[out_idx++] = base64url_alphabet[triple & 0x3F];
        }
    }
    out[out_idx] = '\0';
}

// Helper: Constant-time comparison
static bool constant_time_compare(const char* a, const char* b, size_t len_a, size_t len_b) {
    if (len_a != len_b) return false;
    
    volatile unsigned char result = 0;
    for (size_t i = 0; i < len_a; i++) {
        result |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return result == 0;
}

// Helper to create a string object with explicit length (bypassing strlen)
// This mirrors the static string_create in vm.c
static StringObject* create_string_with_length(const char* chars, int length) {
    StringObject* str = (StringObject*)malloc(sizeof(StringObject) + length + 1);
    if (!str) return NULL;
    str->header.ref_count = 1;
    str->header.type = VAL_STRING;
    str->length = length;
    str->hash_computed = false;
    str->hash = 0;
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    return str;
}

bool secrets_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;

    // --- secrets.token_bytes(n) ---
    if (strcmp(name, "secrets.token_bytes") == 0) {
        if (arg_count != 1 || args[0].type != VAL_NUMBER) {
            *result = vm_make_bool(false);
            return true;
        }
        int n = (int)args[0].number;
        if (n < 0) {
            *result = vm_make_bool(false);
            return true;
        }
        
        unsigned char* buffer = (unsigned char*)malloc(n > 0 ? n : 1);
        if (!buffer) {
            *result = vm_make_bool(false);
            return true;
        }
        
        get_secure_bytes(buffer, n);
        
        // Create string object manually to support null bytes
        StringObject* str_obj = create_string_with_length((const char*)buffer, n);
        free(buffer);
        
        if (!str_obj) {
            *result = vm_make_bool(false);
            return true;
        }
        
        result->type = VAL_STRING;
        result->string = str_obj;
        return true;
    }

    // --- secrets.token_hex(nbytes) ---
    if (strcmp(name, "secrets.token_hex") == 0) {
        int nbytes = 16; // Default
        if (arg_count == 1) {
            if (args[0].type != VAL_NUMBER) {
                *result = vm_make_bool(false);
                return true;
            }
            nbytes = (int)args[0].number;
            if (nbytes < 0) {
                *result = vm_make_bool(false);
                return true;
            }
        } else if (arg_count > 1) {
            *result = vm_make_bool(false);
            return true;
        }

        unsigned char* buffer = (unsigned char*)malloc(nbytes > 0 ? nbytes : 1);
        if (!buffer) {
            *result = vm_make_bool(false);
            return true;
        }
        get_secure_bytes(buffer, nbytes);
        
        char* hex_str = (char*)malloc(nbytes * 2 + 1);
        if (!hex_str) {
            free(buffer);
            *result = vm_make_bool(false);
            return true;
        }
        
        bytes_to_hex(buffer, nbytes, hex_str);
        free(buffer);
        
        *result = vm_make_string(hex_str);
        free(hex_str);
        return true;
    }

    // --- secrets.token_urlsafe(nbytes) ---
    if (strcmp(name, "secrets.token_urlsafe") == 0) {
        int nbytes = 16; // Default
        if (arg_count == 1) {
            if (args[0].type != VAL_NUMBER) {
                *result = vm_make_bool(false);
                return true;
            }
            nbytes = (int)args[0].number;
            if (nbytes < 0) {
                *result = vm_make_bool(false);
                return true;
            }
        } else if (arg_count > 1) {
            *result = vm_make_bool(false);
            return true;
        }

        unsigned char* buffer = (unsigned char*)malloc(nbytes > 0 ? nbytes : 1);
        if (!buffer) {
            *result = vm_make_bool(false);
            return true;
        }
        get_secure_bytes(buffer, nbytes);
        
        // Base64 output size is approx 4/3 of input. 
        // Max size: ceil(nbytes / 3) * 4
        size_t out_size = ((nbytes + 2) / 3) * 4 + 1;
        char* b64_str = (char*)malloc(out_size);
        if (!b64_str) {
            free(buffer);
            *result = vm_make_bool(false);
            return true;
        }
        
        bytes_to_base64url(buffer, nbytes, b64_str);
        free(buffer);
        
        *result = vm_make_string(b64_str);
        free(b64_str);
        return true;
    }

    // --- secrets.choice(sequence) ---
    if (strcmp(name, "secrets.choice") == 0) {
        if (arg_count != 1 || args[0].type != VAL_TABLE) {
            *result = vm_make_bool(false);
            return true;
        }
        
        Table* t = args[0].table;
        int size = table_size(t);
        if (size == 0) {
            *result = vm_make_bool(false);
            return true;
        }
        
        int count;
        char** keys = table_keys(t, &count);
        if (!keys || count == 0) {
            *result = vm_make_bool(false);
            return true;
        }
        
        // Use secure random index
        unsigned char rb;
        get_secure_bytes(&rb, 1);
        int idx = rb % count;
        
        Value val;
        if (table_get(t, keys[idx], &val)) {
            *result = val; // table_get already incref'd
        } else {
            *result = vm_make_bool(false);
        }
        
        free(keys);
        return true;
    }

    // --- secrets.randbelow(n) ---
    if (strcmp(name, "secrets.randbelow") == 0) {
        if (arg_count != 1 || args[0].type != VAL_NUMBER) {
            *result = vm_make_bool(false);
            return true;
        }
        
        int n = (int)args[0].number;
        if (n <= 0) {
            *result = vm_make_bool(false);
            return true;
        }
        
        unsigned char rb;
        get_secure_bytes(&rb, 1);
        *result = vm_make_number((double)(rb % n));
        return true;
    }

    // --- secrets.randbits(k) ---
    if (strcmp(name, "secrets.randbits") == 0) {
        if (arg_count != 1 || args[0].type != VAL_NUMBER) {
            *result = vm_make_bool(false);
            return true;
        }
        
        int k = (int)args[0].number;
        if (k < 0 || k > 32) { // Limit to 32 bits for simplicity in double storage
            *result = vm_make_bool(false);
            return true;
        }
        
        unsigned int result_val = 0;
        int bytes_needed = (k + 7) / 8;
        unsigned char* buffer = (unsigned char*)malloc(bytes_needed);
        if (!buffer) {
            *result = vm_make_bool(false);
            return true;
        }
        
        get_secure_bytes(buffer, bytes_needed);
        
        for (int i = 0; i < bytes_needed; i++) {
            result_val = (result_val << 8) | buffer[i];
        }
        
        // Mask off extra bits if k is not a multiple of 8
        if (k % 8 != 0) {
            result_val &= ((1U << k) - 1);
        }
        
        free(buffer);
        *result = vm_make_number((double)result_val);
        return true;
    }

    // --- secrets.compare_digest(a, b) ---
    if (strcmp(name, "secrets.compare_digest") == 0) {
        if (arg_count != 2) {
            *result = vm_make_bool(false);
            return true;
        }
        
        // Both must be strings
        if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
            *result = vm_make_bool(false);
            return true;
        }
        
        StringObject* sa = args[0].string;
        StringObject* sb = args[1].string;
        
        bool match = constant_time_compare(sa->chars, sb->chars, sa->length, sb->length);
        *result = vm_make_bool(match);
        return true;
    }

    return false;
}