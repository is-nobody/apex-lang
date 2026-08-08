// source/libraries/random_module.c
// Implementation of Random Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "random_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846  // pi constant if not defined
#endif

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

// dispatcher for random number generation built-in functions
bool random_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;                                                              // suppress unused parameter warning
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

    return false;  // not a recognized builtin
}