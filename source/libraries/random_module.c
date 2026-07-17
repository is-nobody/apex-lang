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
#define M_PI 3.14159265358979323846
#endif

// fills a buffer with cryptographically secure random bytes
static void get_secure_bytes(unsigned char* buffer, size_t length) {
#if defined(_WIN32)
    for (size_t i = 0; i < length; i++) {
        unsigned int val;
        rand_s(&val);
        buffer[i] = (unsigned char)(val & 0xFF);
    }
#else
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t read_count = fread(buffer, 1, length, f);
        fclose(f);
        if (read_count == length) return;
    }
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        seeded = 1;
    }
    for (size_t i = 0; i < length; i++)
        buffer[i] = (unsigned char)(rand() % 256);
#endif
}

// converts bytes to a hex string
static void bytes_to_hex(const unsigned char* bytes, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex_chars[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex_chars[bytes[i] & 0xF];
    }
    out[len * 2] = '\0';
}

// constant-time comparison to prevent timing attacks
static bool constant_time_compare(const char* a, const char* b, size_t len_a, size_t len_b) {
    if (len_a != len_b) return false;
    volatile unsigned char result = 0;
    for (size_t i = 0; i < len_a; i++) {
        result |= (unsigned char)a[i] ^ (unsigned char)b[i];
    }
    return result == 0;
}

static int random_seeded = 0;

// ensures the random generator is seeded at least once
static void ensure_seeded() {
    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = 1;
    }
}

// safely extracts a number from a value, returns false on type mismatch
static double get_number_safe(Value v, bool* ok) {
    if (IS_NUMBER(v)) {
        *ok = true;
        return AS_NUMBER(v);
    }
    *ok = false;
    return 0.0;
}

// returns a random integer in [min, max] inclusive
static int randint_range(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    return min + (rand() % (max - min + 1));
}

// generates a gamma-distributed random number (Marsaglia-Tsang method)
static double random_gamma(double shape) {
    if (shape < 1.0) {
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u < 1e-10) u = 1e-10;
        return random_gamma(shape + 1.0) * pow(u, 1.0 / shape);
    }
    double d = shape - 1.0 / 3.0;
    double c = 1.0 / sqrt(9.0 * d);
    while (1) {
        double x, v;
        do {
            double u1 = (double)rand() / ((double)RAND_MAX + 1.0);
            double u2 = (double)rand() / ((double)RAND_MAX + 1.0);
            if (u1 < 1e-10) u1 = 1e-10;
            x = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            v = 1.0 + c * x;
        } while (v <= 0.0);
        v = v * v * v;
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u < 1e-10) u = 1e-10;
        if (u < 1.0 - 0.0331 * x * x * x * x)
            return d * v;
        if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v)))
            return d * v;
    }
}

// helper to create an interned string value
static Value make_string_val(VM* vm, const char* str) {
    (void)vm;
    int len = (int)strlen(str);
    if (len >= 16 && len <= 64 && vm->intern_table.count < 50000) {
        return MAKE_STRING(string_intern(&vm->intern_table, str, len));
    }
    return MAKE_STRING(string_create(str, len));
}

// dispatcher for random number generation built-in functions
bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    ensure_seeded();

    if (strcmp(name, "random.random") == 0) {
        if (arg_count != 0) { *result = MAKE_NONE(); return true; }
        *result = MAKE_NUMBER((double)rand() / ((double)RAND_MAX + 1.0));
        return true;
    }

    if (strcmp(name, "random.randint") == 0) {
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }
        bool ok1, ok2;
        double a = get_number_safe(args[0], &ok1);
        double b = get_number_safe(args[1], &ok2);
        if (!ok1 || !ok2) { *result = MAKE_NONE(); return true; }
        *result = MAKE_NUMBER((double)randint_range((int)a, (int)b));
        return true;
    }

    if (strcmp(name, "random.choice") == 0) {
        if (arg_count != 1 || !IS_TABLE(args[0])) { *result = MAKE_NONE(); return true; }
        Table* t = AS_TABLE(args[0]);
        int size = table_size(t);
        if (size == 0) { *result = MAKE_NONE(); return true; }
        int count;
        Value* keys = table_keys(t, &count);
        if (!keys || count == 0) { *result = MAKE_NONE(); return true; }
        int idx = rand() % count;
        Value val;
        if (table_get(t, keys[idx], &val)) {
            *result = val;
        } else {
            *result = MAKE_NONE();
        }
        for(int i=0; i<count; i++) value_decref(keys[i]);
        free(keys);
        return true;
    }

    if (strcmp(name, "random.shuffle") == 0) {
        if (arg_count != 1 || !IS_TABLE(args[0])) { *result = MAKE_NONE(); return true; }
        Table* t = AS_TABLE(args[0]);
        int size = table_size(t);
        if (size <= 1) { *result = MAKE_NONE(); return true; }
        Value vi, vj;
        for (int i = size; i > 1; i--) {
            int j = rand() % i + 1;
            Value ki = MAKE_NUMBER((double)i);
            Value kj = MAKE_NUMBER((double)j);
            bool got_i = table_get(t, ki, &vi);
            bool got_j = table_get(t, kj, &vj);
            if (got_i && got_j) {
                table_set(t, ki, vj);
                table_set(t, kj, vi);
                value_decref(vi);
                value_decref(vj);
            } else if (got_i) {
                table_set(t, kj, vi);
                table_remove(t, ki);
                value_decref(vi);
            } else if (got_j) {
                table_set(t, ki, vj);
                table_remove(t, kj);
                value_decref(vj);
            }
            value_decref(ki);
            value_decref(kj);
        }
        *result = MAKE_NONE();
        return true;
    }

    if (strcmp(name, "random.sample") == 0) {
        if (arg_count != 2 || !IS_TABLE(args[0]) || !IS_NUMBER(args[1])) {
            *result = MAKE_NONE(); return true;
        }
        Table* src = AS_TABLE(args[0]);
        int k = (int)AS_NUMBER(args[1]);
        int size = table_size(src);
        if (k < 0 || k > size) { *result = MAKE_NONE(); return true; }
        if (k == 0) { *result = MAKE_TABLE(table_create(8)); return true; }
        int count;
        Value* keys = table_keys(src, &count);
        if (!keys) { *result = MAKE_NONE(); return true; }
        Table* res_table = table_create(k);
        int* indices = (int*)malloc(sizeof(int) * count);
        for (int i = 0; i < count; i++) indices[i] = i;
        for (int i = 0; i < k; i++) {
            int j = i + (rand() % (count - i));
            int temp = indices[i]; indices[i] = indices[j]; indices[j] = temp;
            Value val;
            if (table_get(src, keys[indices[i]], &val)) {
                Value res_key = MAKE_NUMBER((double)(i + 1));
                table_set(res_table, res_key, val);
                value_decref(res_key);
                value_decref(val);
            }
        }
        free(indices);
        for(int i=0; i<count; i++) value_decref(keys[i]);
        free(keys);
        *result = MAKE_TABLE(res_table);
        return true;
    }

    if (strcmp(name, "random.gauss") == 0) {
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }
        bool ok1, ok2;
        double mu = get_number_safe(args[0], &ok1);
        double sigma = get_number_safe(args[1], &ok2);
        if (!ok1 || !ok2) { *result = MAKE_NONE(); return true; }
        double u1 = (double)rand() / ((double)RAND_MAX + 1.0);
        double u2 = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u1 < 1e-10) u1 = 1e-10;
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        *result = MAKE_NUMBER(mu + sigma * z0);
        return true;
    }

    if (strcmp(name, "random.seed") == 0) {
        if (arg_count == 1 && IS_NUMBER(args[0])) {
            srand((unsigned int)AS_NUMBER(args[0]));
            random_seeded = 1;
        } else {
            srand((unsigned int)time(NULL));
        }
        *result = MAKE_NONE();
        return true;
    }

    if (strcmp(name, "random.triangular") == 0) {
        double low = 0.0, high = 1.0, mode = 0.5;
        if (arg_count >= 1) {
            if (!IS_NUMBER(args[0])) { *result = MAKE_NONE(); return true; }
            low = AS_NUMBER(args[0]);
        }
        if (arg_count >= 2) {
            if (!IS_NUMBER(args[1])) { *result = MAKE_NONE(); return true; }
            high = AS_NUMBER(args[1]);
        }
        if (arg_count >= 3) {
            if (!IS_NUMBER(args[2])) { *result = MAKE_NONE(); return true; }
            mode = AS_NUMBER(args[2]);
        }
        if (high == low) { *result = MAKE_NUMBER(low); return true; }
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        double cdf_mode = (mode - low) / (high - low);
        double result_val;
        if (u < cdf_mode) {
            result_val = low + sqrt(u * (high - low) * (mode - low));
        } else {
            result_val = high - sqrt((1.0 - u) * (high - low) * (high - mode));
        }
        *result = MAKE_NUMBER(result_val);
        return true;
    }

    if (strcmp(name, "random.expovariate") == 0) {
        if (arg_count != 1 || !IS_NUMBER(args[0])) { *result = MAKE_NONE(); return true; }
        double lambd = AS_NUMBER(args[0]);
        if (lambd == 0.0) { *result = MAKE_NONE(); return true; }
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u < 1e-10) u = 1e-10;
        *result = MAKE_NUMBER(-log(u) / lambd);
        return true;
    }

    if (strcmp(name, "random.betavariate") == 0) {
        if (arg_count != 2) { *result = MAKE_NONE(); return true; }
        bool ok1, ok2;
        double alpha = get_number_safe(args[0], &ok1);
        double beta = get_number_safe(args[1], &ok2);
        if (!ok1 || !ok2 || alpha <= 0.0 || beta <= 0.0) {
            *result = MAKE_NONE();
            return true;
        }
        double x = random_gamma(alpha);
        double y = random_gamma(beta);
        if (x + y == 0.0) {
            *result = MAKE_NONE();
            return true;
        }
        *result = MAKE_NUMBER(x / (x + y));
        return true;
    }

    if (strcmp(name, "random.secure_token_hex") == 0) {
        int nbytes = 16;
        if (arg_count == 1) {
            if (!IS_NUMBER(args[0])) {
                *result = MAKE_NONE();
                return true;
            }
            nbytes = (int)AS_NUMBER(args[0]);
            if (nbytes < 0) {
                *result = MAKE_NONE();
                return true;
            }
        } else if (arg_count > 1) {
            *result = MAKE_NONE();
            return true;
        }
        unsigned char* buffer = (unsigned char*)malloc(nbytes > 0 ? nbytes : 1);
        if (!buffer) {
            *result = MAKE_NONE();
            return true;
        }
        get_secure_bytes(buffer, nbytes);
        char* hex_str = (char*)malloc(nbytes * 2 + 1);
        if (!hex_str) {
            free(buffer);
            *result = MAKE_NONE();
            return true;
        }
        bytes_to_hex(buffer, nbytes, hex_str);
        free(buffer);
        *result = make_string_val(vm, hex_str);
        free(hex_str);
        return true;
    }

    if (strcmp(name, "random.secure_randint") == 0) {
        if (arg_count != 1 || !IS_NUMBER(args[0])) {
            *result = MAKE_NONE();
            return true;
        }
        int n = (int)AS_NUMBER(args[0]);
        if (n <= 0) {
            *result = MAKE_NONE();
            return true;
        }
        unsigned char rb;
        get_secure_bytes(&rb, 1);
        *result = MAKE_NUMBER((double)(rb % n));
        return true;
    }

    if (strcmp(name, "random.compare_digest") == 0) {
        if (arg_count != 2) {
            *result = MAKE_BOOL(false);
            return true;
        }
        if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
            *result = MAKE_BOOL(false);
            return true;
        }
        StringObject* sa = AS_STRING(args[0]);
        StringObject* sb = AS_STRING(args[1]);
        bool match = constant_time_compare(sa->chars, sb->chars, sa->length, sb->length);
        *result = MAKE_BOOL(match);
        return true;
    }

    return false;
}