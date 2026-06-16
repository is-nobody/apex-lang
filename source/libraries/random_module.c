#include "random_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Internal state for seed
static int random_seeded = 0;

// Helper to ensure srand is called at least once
static void ensure_seeded() {
    if (!random_seeded) {
        srand((unsigned int)time(NULL));
        random_seeded = 1;
    }
}

// Helper to get double from Value, return 0.0 and set error flag if not number
static double get_number_safe(Value* v, bool* ok) {
    if (v->type == VAL_NUMBER) {
        *ok = true;
        return v->number;
    }
    *ok = false;
    return 0.0;
}

// Helper to get integer range [min, max] inclusive
static int randint_range(int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    return min + (rand() % (max - min + 1));
}

// Gamma distribution generator (Marsaglia and Tsang's method)
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

bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    ensure_seeded();

    // --- random.random() ---
    if (strcmp(name, "random.random") == 0) {
        if (arg_count != 0) { *result = vm_make_bool(false); return true; }
        *result = vm_make_number((double)rand() / ((double)RAND_MAX + 1.0));
        return true;
    }

    // --- random.randint(a, b) ---
    if (strcmp(name, "random.randint") == 0) {
        if (arg_count != 2) { *result = vm_make_bool(false); return true; }
        bool ok1, ok2;
        double a = get_number_safe(&args[0], &ok1);
        double b = get_number_safe(&args[1], &ok2);
        if (!ok1 || !ok2) { *result = vm_make_bool(false); return true; }
        *result = vm_make_number((double)randint_range((int)a, (int)b));
        return true;
    }

    // --- random.choice(seq) ---
    if (strcmp(name, "random.choice") == 0) {
        if (arg_count != 1 || args[0].type != VAL_TABLE) { *result = vm_make_bool(false); return true; }
        
        Table* t = args[0].table;
        int size = table_size(t);
        if (size == 0) { *result = vm_make_bool(false); return true; }
        
        int count;
        char** keys = table_keys(t, &count);
        if (!keys || count == 0) { *result = vm_make_bool(false); return true; }
        
        int idx = rand() % count;
        Value val;
        if (table_get(t, keys[idx], &val)) {
            *result = val; // table_get already incref'd
        } else {
            *result = vm_make_bool(false);
        }
        free(keys);
        return true;
    }

    // --- random.shuffle(seq) ---
    if (strcmp(name, "random.shuffle") == 0) {
        if (arg_count != 1 || args[0].type != VAL_TABLE) { *result = vm_make_bool(false); return true; }
        
        Table* t = args[0].table;
        int size = table_size(t);
        if (size <= 1) { *result = vm_make_bool(false); return true; }
        
        char ki[32], kj[32];
        Value vi, vj;
        
        for (int i = size; i > 1; i--) {
            int j = rand() % i + 1;
            
            snprintf(ki, sizeof(ki), "%d", i);
            snprintf(kj, sizeof(kj), "%d", j);
            
            bool got_i = table_get(t, ki, &vi);
            bool got_j = table_get(t, kj, &vj);
            
            if (got_i && got_j) {
                table_set(t, ki, vj);
                table_set(t, kj, vi);
                vm_free_value(&vi);
                vm_free_value(&vj);
            } else if (got_i) {
                 table_set(t, kj, vi);
                 table_remove(t, ki);
                 vm_free_value(&vi);
            } else if (got_j) {
                 table_set(t, ki, vj);
                 table_remove(t, kj);
                 vm_free_value(&vj);
            }
        }
        *result = vm_make_bool(false);
        return true;
    }

    // --- random.sample(seq, k) ---
    if (strcmp(name, "random.sample") == 0) {
        if (arg_count != 2 || args[0].type != VAL_TABLE || args[1].type != VAL_NUMBER) {
            *result = vm_make_bool(false); return true;
        }
        
        Table* src = args[0].table;
        int k = (int)args[1].number;
        int size = table_size(src);
        
        if (k < 0 || k > size) { *result = vm_make_bool(false); return true; }
        if (k == 0) { *result = vm_make_table(); return true; }
        
        int count;
        char** keys = table_keys(src, &count);
        if (!keys) { *result = vm_make_bool(false); return true; }
        
        Table* res_table = table_create(k);
        int* indices = (int*)malloc(sizeof(int) * count);
        for (int i = 0; i < count; i++) indices[i] = i;
        
        for (int i = 0; i < k; i++) {
            int j = i + (rand() % (count - i));
            int temp = indices[i]; indices[i] = indices[j]; indices[j] = temp;
            
            Value val;
            if (table_get(src, keys[indices[i]], &val)) {
                char res_key[32];
                snprintf(res_key, sizeof(res_key), "%d", i + 1);
                table_set(res_table, res_key, val);
                vm_free_value(&val);
            }
        }
        
        free(indices);
        free(keys);
        result->type = VAL_TABLE;
        result->table = res_table;
        return true;
    }

    // --- random.choices(seq, k=1) ---
    if (strcmp(name, "random.choices") == 0) {
        if (arg_count < 1 || arg_count > 2 || args[0].type != VAL_TABLE) {
            *result = vm_make_bool(false); return true;
        }
        
        int k = 1;
        if (arg_count == 2) {
            if (args[1].type != VAL_NUMBER) { *result = vm_make_bool(false); return true; }
            k = (int)args[1].number;
            if (k < 0) k = 0;
        }
        
        Table* src = args[0].table;
        int size = table_size(src);
        if (size == 0) { *result = vm_make_table(); return true; }
        
        int count;
        char** keys = table_keys(src, &count);
        if (!keys) { *result = vm_make_bool(false); return true; }
        
        Table* res_table = table_create(k);
        for (int i = 0; i < k; i++) {
            int idx = rand() % count;
            Value val;
            if (table_get(src, keys[idx], &val)) {
                char res_key[32];
                snprintf(res_key, sizeof(res_key), "%d", i + 1);
                table_set(res_table, res_key, val);
                vm_free_value(&val);
            }
        }
        free(keys);
        result->type = VAL_TABLE;
        result->table = res_table;
        return true;
    }

    // --- random.gauss(mu, sigma) ---
    if (strcmp(name, "random.gauss") == 0) {
        if (arg_count != 2) { *result = vm_make_bool(false); return true; }
        bool ok1, ok2;
        double mu = get_number_safe(&args[0], &ok1);
        double sigma = get_number_safe(&args[1], &ok2);
        if (!ok1 || !ok2) { *result = vm_make_bool(false); return true; }
        
        double u1 = (double)rand() / ((double)RAND_MAX + 1.0);
        double u2 = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u1 < 1e-10) u1 = 1e-10;
        
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        *result = vm_make_number(mu + sigma * z0);
        return true;
    }

    // --- random.seed(a) ---
    if (strcmp(name, "random.seed") == 0) {
        if (arg_count == 1 && args[0].type == VAL_NUMBER) {
            srand((unsigned int)args[0].number);
            random_seeded = 1;
        } else {
            srand((unsigned int)time(NULL));
        }
        *result = vm_make_bool(false);
        return true;
    }

    // --- random.triangular(low, high, mode) ---
    if (strcmp(name, "random.triangular") == 0) {
        double low = 0.0, high = 1.0, mode = 0.5;
        if (arg_count >= 1) {
            if (args[0].type != VAL_NUMBER) { *result = vm_make_bool(false); return true; }
            low = args[0].number;
        }
        if (arg_count >= 2) {
            if (args[1].type != VAL_NUMBER) { *result = vm_make_bool(false); return true; }
            high = args[1].number;
        }
        if (arg_count >= 3) {
            if (args[2].type != VAL_NUMBER) { *result = vm_make_bool(false); return true; }
            mode = args[2].number;
        }
        
        if (high == low) { *result = vm_make_number(low); return true; }
        
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        double cdf_mode = (mode - low) / (high - low);
        double result_val;
        if (u < cdf_mode) {
            result_val = low + sqrt(u * (high - low) * (mode - low));
        } else {
            result_val = high - sqrt((1.0 - u) * (high - low) * (high - mode));
        }
        *result = vm_make_number(result_val);
        return true;
    }

    // --- random.expovariate(lambd) ---
    if (strcmp(name, "random.expovariate") == 0) {
        if (arg_count != 1 || args[0].type != VAL_NUMBER) { *result = vm_make_bool(false); return true; }
        double lambd = args[0].number;
        if (lambd == 0.0) { *result = vm_make_bool(false); return true; }
        
        double u = (double)rand() / ((double)RAND_MAX + 1.0);
        if (u < 1e-10) u = 1e-10;
        *result = vm_make_number(-log(u) / lambd);
        return true;
    }

    // --- random.betavariate(alpha, beta) ---
    if (strcmp(name, "random.betavariate") == 0) {
        if (arg_count != 2) { *result = vm_make_bool(false); return true; }
        bool ok1, ok2;
        double alpha = get_number_safe(&args[0], &ok1);
        double beta = get_number_safe(&args[1], &ok2);
        if (!ok1 || !ok2 || alpha <= 0.0 || beta <= 0.0) { *result = vm_make_bool(false); return true; }
        
        double x = random_gamma(alpha);
        double y = random_gamma(beta);
        if (x + y == 0.0) { *result = vm_make_bool(false); return true; }
        
        *result = vm_make_number(x / (x + y));
        return true;
    }

    return false;
}