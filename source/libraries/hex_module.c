// source/libraries/hex_module.c
// Implementation of Hex Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "hex_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// decodes a single hex character, returns -1 on invalid
static int hex_decode_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';                                // digit
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;                           // uppercase hex
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;                           // lowercase hex
    return -1;                                                               // invalid
}

// main dispatcher for hex module built-in functions
bool hex_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "hex.encode") == 0) {                                       // hex encode
        if (arg_count < 1 || !IS_STRING(args[0])) {                              // validate string
            *result = MAKE_NONE();                                               // invalid
            return true;                                                         // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                            // input string
        
        int byte_count = input_str->length;                                      // number of input bytes
        int out_size = byte_count * 2 + 1;                                       // output size (2 hex chars per byte)
        char* out = (char*)malloc(out_size);                                     // allocate output
        if (!out) { *result = MAKE_NONE(); return true; }                        // allocation failed
        
        int out_idx = 0;                                                         // output index
        for (int i = 0; i < byte_count; i++) {                                   // iterate over input bytes
            unsigned char byte = (unsigned char)input_str->chars[i];             // get byte
            out[out_idx++] = "0123456789abcdef"[byte >> 4];                      // high nibble (lowercase)
            out[out_idx++] = "0123456789abcdef"[byte & 0x0F];                    // low nibble (lowercase)
        }
        out[out_idx] = '\0';                                                     // null terminate
        
        *result = MAKE_STRING(string_intern(&vm->intern_table, out, out_idx));   // intern result
        free(out);                                                               // free output
        return true;                                                             // builtin handled
    }
    
    if (strcmp(name, "hex.decode") == 0) {                                       // hex decode
        if (arg_count < 1 || !IS_STRING(args[0])) {                              // validate string argument
            *result = MAKE_NONE();                                               // invalid input
            return true;                                                         // builtin handled
        }
        StringObject* input_str = AS_STRING(args[0]);                            // input string object
        const char* hex = input_str->chars;                                      // raw hex characters
        int hex_len = input_str->length;                                         // input length in bytes
        
        int valid_hex = 0;                                                       // count of valid hex nibbles
        for (int i = 0; i < hex_len; i++) {                                      // scan entire input
            if (hex_decode_nibble(hex[i]) >= 0) valid_hex++;                     // count each valid hex char
        }
        
        if (valid_hex == 0) {                                                    // no valid hex characters found
            *result = MAKE_STRING(string_intern(&vm->intern_table, "", 0));      // return empty string
            return true;                                                         // builtin handled
        }
        
        int out_size = (valid_hex + 1) / 2;                                      // number of output bytes
        unsigned char* out = (unsigned char*)malloc(out_size);                   // allocate exact size (no null terminator needed)
        if (!out) { *result = MAKE_NONE(); return true; }                        // allocation failed
        
        int out_len = 0;                                                         // current output length
        int nibble_count = 0;                                                    // 0 = waiting for high nibble, 1 = waiting for low nibble
        unsigned char current_byte = 0;                                          // byte being assembled from nibbles
        
        // second pass: decode hex characters into bytes
        for (int i = 0; i < hex_len; i++) {                                      // iterate over input characters
            int nibble = hex_decode_nibble(hex[i]);                              // decode character to 4-bit value (-1 if invalid)
            if (nibble < 0) continue;                                            // skip whitespace, invalid chars gracefully
            
            if (nibble_count == 0) {                                             // first nibble of a byte pair
                current_byte = (unsigned char)(nibble << 4);                     // set high 4 bits
                nibble_count = 1;                                                // expect low nibble next
            } else {                                                             // second nibble of a byte pair
                current_byte |= (unsigned char)nibble;                           // set low 4 bits
                out[out_len++] = current_byte;                                   // emit completed byte
                nibble_count = 0;                                                // reset for next byte pair
            }
        }
        
        if (nibble_count == 1) {                                                 // unpaired high nibble remaining
            out[out_len++] = current_byte;                                       // emit byte with low nibble as 0
        }
        
        *result = MAKE_STRING(string_intern(&vm->intern_table, (char*)out, out_len));
        free(out);                                                               // free temporary output buffer
        return true;                                                             // builtin handled
    }

    return false;                                                                // not a recognized builtin
}