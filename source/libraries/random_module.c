// source/libraries/random_module.c
// Implementation of Random Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "random_module.h"
#include "vm.h"
#ifdef _WIN32
#define _CRT_RAND_S
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846  // pi constant if not defined
#endif

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

static int random_seeded = 0;                                 // seed flag

// ensures the random generator is seeded at least once
static void ensure_seeded() {
    if (!random_seeded) {                                     // not seeded
        srand((unsigned int)time(NULL));                      // seed from time
        random_seeded = 1;                                    // mark seeded
    }
}

// safely extracts a number from a value, returns false on type mismatch
static double get_number_safe(Value v, bool* ok) {
    if (IS_NUMBER(v)) {                                       // is number
        *ok = true;                                           // valid
        return AS_NUMBER(v);                                  // return value
    }
    *ok = false;                                              // invalid
    return 0.0;                                               // fallback
}

// returns a random integer in [min, max] inclusive
static int randint_range(int min, int max) {
    if (min > max) {                                          // out of order
        int temp = min;                                       // swap
        min = max;                                            // min becomes max
        max = temp;                                           // max becomes min
    }
    return min + (rand() % (max - min + 1));                  // random in range
}

// generates a gamma-distributed random number (Marsaglia-Tsang method)
static double random_gamma(double shape) {
    if (shape < 1.0) {
        double u = (double)rand() / ((double)RAND_MAX + 1.0);       // uniform random
        if (u < 1e-10) u = 1e-10;                                   // avoid zero
        return random_gamma(shape + 1.0) * pow(u, 1.0 / shape);     // gamma + 1 method
    }
    double d = shape - 1.0 / 3.0;                                   // Marsaglia-Tsang d
    double c = 1.0 / sqrt(9.0 * d);                                 // Marsaglia-Tsang c
    while (1) {                                                     // acceptance-rejection loop
        double x, v;
        do {
            double u1 = (double)rand() / ((double)RAND_MAX + 1.0);  // uniform 1
            double u2 = (double)rand() / ((double)RAND_MAX + 1.0);  // uniform 2
            if (u1 < 1e-10) u1 = 1e-10;                             // avoid zero
            x = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);        // normal sample
            v = 1.0 + c * x;                                        // transformed value
        } while (v <= 0.0);                                         // ensure positive
        v = v * v * v;                                              // cube
        double u = (double)rand() / ((double)RAND_MAX + 1.0);       // acceptance check
        if (u < 1e-10) u = 1e-10;                                   // avoid zero
        if (u < 1.0 - 0.0331 * x * x * x * x)                       // quick accept
            return d * v;
        if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v)))          // slow accept
            return d * v;
    }
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                      // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));  // intern and box as value
}

// dispatcher for random number generation built-in functions
bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    ensure_seeded();                                                       // ensure generator seeded

    if (strcmp(name, "random.random") == 0) {                              // uniform [0,1)
        if (arg_count != 0) { *result = MAKE_NONE(); return true; }        // no args expected
        *result = MAKE_NUMBER((double)rand() / ((double)RAND_MAX + 1.0));  // random in [0,1)
        return true;                                                       // builtin handled
    }

    if (strcmp(name, "random.randint") == 0) {                            // random integer in range
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }       // need 2 args
        bool ok1, ok2;                                                    // validity flags
        double a = get_number_safe(args[0], &ok1);                        // get min
        double b = get_number_safe(args[1], &ok2);                        // get max
        if (!ok1 || !ok2) { *result = MAKE_NONE(); return true; }         // invalid numbers
        *result = MAKE_NUMBER((double)randint_range((int)a, (int)b));     // random integer
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "random.choice") == 0) {                             // pick random element
        if (arg_count != 1 || !IS_TABLE(args[0])) { *result = MAKE_NONE(); return true; }  // validate table
        Table* t = AS_TABLE(args[0]);                                     // unwrap table
        int size = table_size(t);                                         // table size
        if (size == 0) { *result = MAKE_NONE(); return true; }            // empty table
        int count;                                                        // key count
        Value* keys = table_keys(t, &count);                              // get all keys
        if (!keys || count == 0) { *result = MAKE_NONE(); return true; }  // no keys
        int idx = rand() % count;                                         // random index
        Value val;                                                        // value storage
        if (table_get(t, keys[idx], &val)) {                              // get random value
            *result = val;                                                // return value
        } else {
            *result = MAKE_NONE();                                        // not found
        }
        for(int i=0; i<count; i++) value_decref(keys[i]);                 // release keys
        free(keys);                                                       // free key array
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "random.shuffle") == 0) {                            // shuffle table in place
        if (arg_count != 1 || !IS_TABLE(args[0])) { *result = MAKE_NONE(); return true; } // validate table
        Table* t = AS_TABLE(args[0]);                           // unwrap table
        int size = table_size(t);                               // table size
        if (size <= 1) { *result = MAKE_NONE(); return true; }  // nothing to shuffle
        Value vi, vj;                                           // value buffers
        for (int i = size; i > 1; i--) {                        // Fisher-Yates shuffle
            int j = rand() % i + 1;                             // random index
            Value ki = MAKE_NUMBER((double)i);                  // key i
            Value kj = MAKE_NUMBER((double)j);                  // key j
            bool got_i = table_get(t, ki, &vi);                 // get value at i
            bool got_j = table_get(t, kj, &vj);                 // get value at j
            if (got_i && got_j) {                               // both exist
                table_set(t, ki, vj);                           // swap
                table_set(t, kj, vi);                           // swap
                value_decref(vi);                               // release old i
                value_decref(vj);                               // release old j
            } else if (got_i) {                                 // only i exists
                table_set(t, kj, vi);                           // move i to j
                table_remove(t, ki);                            // remove i
                value_decref(vi);                               // release value
            } else if (got_j) {                                 // only j exists
                table_set(t, ki, vj);                           // move j to i
                table_remove(t, kj);                            // remove j
                value_decref(vj);                               // release value
            }
            value_decref(ki);                                   // release key i
            value_decref(kj);                                   // release key j
        }
        *result = MAKE_NONE();                                  // return none
        return true;                                            // builtin handled
    }

    if (strcmp(name, "random.sample") == 0) {                                   // sample without replacement
        if (arg_count != 2 || !IS_TABLE(args[0]) || !IS_NUMBER(args[1])) {      // validate args
            *result = MAKE_NONE(); return true;
        }
        Table* src = AS_TABLE(args[0]);                                         // source table
        int k = (int)AS_NUMBER(args[1]);                                        // sample size
        int size = table_size(src);                                             // table size
        if (k < 0 || k > size) { *result = MAKE_NONE(); return true; }          // invalid sample size
        if (k == 0) { *result = MAKE_TABLE(table_create(8)); return true; }     // empty sample
        int count;                                                              // key count
        Value* keys = table_keys(src, &count);                                  // get all keys
        if (!keys) { *result = MAKE_NONE(); return true; }                      // no keys
        Table* res_table = table_create(k);                                     // result table
        int* indices = (int*)malloc(sizeof(int) * count);                       // index array
        for (int i = 0; i < count; i++) indices[i] = i;                         // initialize indices
        for (int i = 0; i < k; i++) {                                           // select k random
            int j = i + (rand() % (count - i));                                 // random index
            int temp = indices[i]; indices[i] = indices[j]; indices[j] = temp;  // swap
            Value val;                                                          // value storage
            if (table_get(src, keys[indices[i]], &val)) {                       // get value
                Value res_key = MAKE_NUMBER((double)(i + 1));                   // result key
                table_set(res_table, res_key, val);                             // store value
                value_decref(res_key);                                          // release key
                value_decref(val);                                              // release value
            }
        }
        free(indices);                                               // free index array
        for(int i=0; i<count; i++) value_decref(keys[i]);            // release keys
        free(keys);                                                  // free key array
        *result = MAKE_TABLE(res_table);                             // return result
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.gauss") == 0) {                         // normal distribution
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }  // need 2 args
        bool ok1, ok2;                                               // validity flags
        double mu = get_number_safe(args[0], &ok1);                  // mean
        double sigma = get_number_safe(args[1], &ok2);               // standard deviation
        if (!ok1 || !ok2) { *result = MAKE_NONE(); return true; }    // invalid numbers
        double u1 = (double)rand() / ((double)RAND_MAX + 1.0);       // uniform 1
        double u2 = (double)rand() / ((double)RAND_MAX + 1.0);       // uniform 2
        if (u1 < 1e-10) u1 = 1e-10;                                  // avoid zero
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);     // box-muller
        *result = MAKE_NUMBER(mu + sigma * z0);                      // return normal
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.seed") == 0) {                          // seed generator
        if (arg_count == 1 && IS_NUMBER(args[0])) {                  // seed provided
            srand((unsigned int)AS_NUMBER(args[0]));                 // set seed
            random_seeded = 1;                                       // mark seeded
        } else {                                                     // no seed
            srand((unsigned int)time(NULL));                         // seed from time
        }
        *result = MAKE_NONE();                                       // return none
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.triangular") == 0) {                                // triangular distribution
        double low = 0.0, high = 1.0, mode = 0.5;                                // defaults
        if (arg_count >= 1) {                                                    // low provided
            if (!IS_NUMBER(args[0])) { *result = MAKE_NONE(); return true; }     // validate
            low = AS_NUMBER(args[0]);                                            // set low
        }
        if (arg_count >= 2) {                                                    // high provided
            if (!IS_NUMBER(args[1])) { *result = MAKE_NONE(); return true; }     // validate
            high = AS_NUMBER(args[1]);                                           // set high
        }
        if (arg_count >= 3) {                                                    // mode provided
            if (!IS_NUMBER(args[2])) { *result = MAKE_NONE(); return true; }     // validate
            mode = AS_NUMBER(args[2]);                                           // set mode
        }
        if (high == low) { *result = MAKE_NUMBER(low); return true; }            // degenerate case
        double u = (double)rand() / ((double)RAND_MAX + 1.0);                    // uniform
        double cdf_mode = (mode - low) / (high - low);                           // mode cdf
        double result_val;                                                       // result
        if (u < cdf_mode) {                                                      // left side
            result_val = low + sqrt(u * (high - low) * (mode - low));            // inverse cdf left
        } else {                                                                 // right side
            result_val = high - sqrt((1.0 - u) * (high - low) * (high - mode));  // inverse cdf right
        }
        *result = MAKE_NUMBER(result_val);                           // return value
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.expovariate") == 0) {                   // exponential distribution
        if (arg_count != 1 || !IS_NUMBER(args[0])) { *result = MAKE_NONE(); return true; } // validate
        double lambd = AS_NUMBER(args[0]);                           // rate parameter
        if (lambd == 0.0) { *result = MAKE_NONE(); return true; }    // invalid rate
        double u = (double)rand() / ((double)RAND_MAX + 1.0);        // uniform
        if (u < 1e-10) u = 1e-10;                                    // avoid zero
        *result = MAKE_NUMBER(-log(u) / lambd);                      // inverse cdf
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.betavariate") == 0) {                   // beta distribution
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }  // need 2 args
        bool ok1, ok2;                                               // validity flags
        double alpha = get_number_safe(args[0], &ok1);               // alpha parameter
        double beta = get_number_safe(args[1], &ok2);                // beta parameter
        if (!ok1 || !ok2 || alpha <= 0.0 || beta <= 0.0) {           // invalid parameters
            *result = MAKE_NONE();
            return true;
        }
        double x = random_gamma(alpha);                              // gamma alpha
        double y = random_gamma(beta);                               // gamma beta
        if (x + y == 0.0) {                                          // both zero
            *result = MAKE_NONE();
            return true;
        }
        *result = MAKE_NUMBER(x / (x + y));                          // beta = gamma ratio
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.secure_token_hex") == 0) {              // secure hex token
        int nbytes = 16;                                             // default bytes
        if (arg_count == 1) {                                        // size provided
            if (!IS_NUMBER(args[0])) {                               // validate
                *result = MAKE_NONE();
                return true;
            }
            nbytes = (int)AS_NUMBER(args[0]);                        // set size
            if (nbytes < 0) {                                        // invalid negative
                *result = MAKE_NONE();
                return true;
            }
        } else if (arg_count > 1) {                                  // too many args
            *result = MAKE_NONE();
            return true;
        }
        unsigned char* buffer = (unsigned char*)malloc(nbytes > 0 ? nbytes : 1);  // allocate buffer
        if (!buffer) {                                                            // allocation failed
            *result = MAKE_NONE();
            return true;
        }
        get_secure_bytes(buffer, nbytes);                 // fill with secure bytes
        char* hex_str = (char*)malloc(nbytes * 2 + 1);    // allocate hex string
        if (!hex_str) {                                   // allocation failed
            free(buffer);                                 // free buffer
            *result = MAKE_NONE();
            return true;
        }
        bytes_to_hex(buffer, nbytes, hex_str);            // convert to hex
        free(buffer);                                     // free buffer
        *result = make_string_val(vm, hex_str);           // return hex string
        free(hex_str);                                    // free hex string
        return true;                                      // builtin handled
    }

    if (strcmp(name, "random.secure_randint") == 0) {     // secure random integer
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

    if (strcmp(name, "random.compare_digest") == 0) {      // constant-time compare
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
    return false;     // not a recognized builtin
}