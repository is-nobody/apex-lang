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

// one-time initializer for the process-wide RNG mutex. libc's srand/rand share a
// single global state, so two ApexStates running in parallel host threads would
// race on it; the mutex serialises every access.
#ifdef _WIN32
static INIT_ONCE g_rng_once = INIT_ONCE_STATIC_INIT;
static ApexMutex g_rng_mutex;
static BOOL CALLBACK rng_once_init(PINIT_ONCE o, PVOID p, PVOID* c) {
    (void)o; (void)p; (void)c;
    InitializeCriticalSection(&g_rng_mutex);
    return TRUE;
}
static void ensure_rng_mutex(void) {
    InitOnceExecuteOnce(&g_rng_once, rng_once_init, NULL, NULL);
}
#else
static pthread_once_t g_rng_once = PTHREAD_ONCE_INIT;
static ApexMutex g_rng_mutex;
static void rng_once_init(void) {
    pthread_mutex_init(&g_rng_mutex, NULL);
}
static void ensure_rng_mutex(void) {
    pthread_once(&g_rng_once, rng_once_init);
}
#endif

static int random_seeded = 0;                                 // seed flag, guarded by g_rng_mutex

// ensures the random generator is seeded at least once; caller must hold g_rng_mutex
static void ensure_seeded_locked(void) {
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
static long long randint_range(long long min, long long max) {
    if (min > max) {                                          // out of order
        long long temp = min;                                 // swap
        min = max;
        max = temp;
    }
    // compute the range as unsigned so max - min + 1 cannot overflow
    unsigned long long range = (unsigned long long)max - (unsigned long long)min + 1ULL;
    unsigned long long r;                                     // sampled offset
    if (range <= (unsigned long long)RAND_MAX) {              // single draw is enough
        r = (unsigned long long)rand() % range;
    } else {                                                  // combine two draws for a wider range
        r = (((unsigned long long)rand() << 15) | (unsigned long long)rand()) % range;
    }
    return min + (long long)r;                                // return random in range
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
    ensure_rng_mutex();                                                    // one-time init of the RNG mutex
    APEX_MUTEX_LOCK(&g_rng_mutex);                                         // serialise all rand/srand access
    ensure_seeded_locked();                                                // ensure generator seeded (locked)

    if (strcmp(name, "random.float") == 0) {                              // uniform [0,1)
        if (arg_count != 0) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // no args expected
        *result = MAKE_NUMBER((double)rand() / ((double)RAND_MAX + 1.0));  // random in [0,1)
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                                   // release lock
        return true;                                                       // builtin handled
    }

    if (strcmp(name, "random.integer") == 0) {                            // random integer in range
        if (arg_count != 2) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // need 2 args
        bool ok1, ok2;                                                    // validity flags
        double a = get_number_safe(args[0], &ok1);                        // get min
        double b = get_number_safe(args[1], &ok2);                        // get max
        if (!ok1 || !ok2) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // invalid numbers
        // reject nan/inf (short-circuits) and out-of-range or non-whole values before the cast
        if (!(a >= -9007199254740992.0 && a <= 9007199254740992.0) ||
            !(b >= -9007199254740992.0 && b <= 9007199254740992.0) ||
            a != (double)(long long)a ||
            b != (double)(long long)b) {
            APEX_MUTEX_UNLOCK(&g_rng_mutex);
            *result = MAKE_NONE();                                        // invalid argument range
            return true;
        }
        *result = MAKE_NUMBER((double)randint_range((long long)a, (long long)b));
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                                   // release lock
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "random.choice") == 0) {                             // pick random element
        if (arg_count != 1 || !IS_TABLE(args[0])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // validate table
        Table* t = AS_TABLE(args[0]);                                     // unwrap table
        int size = table_size(t);                                         // table size
        if (size == 0) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // empty table
        int count;                                                        // key count
        Value* keys = table_keys(t, &count);                              // get all keys
        if (!keys || count == 0) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // no keys
        int idx = rand() % count;                                         // random index
        Value val;                                                        // value storage
        if (table_get(t, keys[idx], &val)) {                              // get random value
            *result = val;                                                // return value
        } else {
            *result = MAKE_NONE();                                        // not found
        }
        for(int i=0; i<count; i++) value_decref(keys[i]);                 // release keys
        free(keys);                                                       // free key array
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                                   // release lock
        return true;                                                      // builtin handled
    }

    if (strcmp(name, "random.shuffle") == 0) {                            // shuffle table in place
        if (arg_count != 1 || !IS_TABLE(args[0])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; } // validate table
        Table* t = AS_TABLE(args[0]);                           // unwrap table
        int size = table_size(t);                               // table size
        if (size <= 1) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // nothing to shuffle
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
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                        // release lock
        return true;                                            // builtin handled
    }

    if (strcmp(name, "random.sample") == 0) {                                   // sample without replacement
        if (arg_count != 2 || !IS_TABLE(args[0]) || !IS_NUMBER(args[1])) {      // validate args
            APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true;
        }
        double k_d = AS_NUMBER(args[1]);                                        // raw sample size
        if (!(k_d >= 0 && k_d <= 2147483647.0) || k_d != (double)(long long)k_d) {  // reject nan/inf/out-of-range/non-whole
            APEX_MUTEX_UNLOCK(&g_rng_mutex);
            *result = MAKE_NONE();
            return true;
        }
        Table* src = AS_TABLE(args[0]);                                         // source table
        int k = (int)k_d;                                                       // sample size
        int size = table_size(src);                                             // table size
        if (k > size) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // invalid sample size
        if (k == 0) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_TABLE(table_create(8)); return true; }  // empty sample
        int count;                                                              // key count
        Value* keys = table_keys(src, &count);                                  // get all keys
        if (!keys) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // no keys
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
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.normal") == 0) {                         // normal distribution
        if (arg_count != 2) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // need 2 args
        bool ok1, ok2;                                               // validity flags
        double mu = get_number_safe(args[0], &ok1);                  // mean
        double sigma = get_number_safe(args[1], &ok2);               // standard deviation
        if (!ok1 || !ok2) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // invalid numbers
        double u1 = (double)rand() / ((double)RAND_MAX + 1.0);       // uniform 1
        double u2 = (double)rand() / ((double)RAND_MAX + 1.0);       // uniform 2
        if (u1 < 1e-10) u1 = 1e-10;                                  // avoid zero
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);     // box-muller
        *result = MAKE_NUMBER(mu + sigma * z0);                      // return normal
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.seed") == 0) {                          // seed generator
        if (arg_count == 1 && IS_NUMBER(args[0])) {                  // seed provided
            double seed_d = AS_NUMBER(args[0]);                      // raw seed value
            if (!(seed_d == seed_d)) seed_d = 0;                     // NaN -> 0
            if (seed_d < 0) seed_d = -seed_d;                        // use magnitude
            if (seed_d > 4294967295.0) seed_d = 4294967295.0;        // clamp to UINT_MAX
            srand((unsigned int)seed_d);                             // set seed
            random_seeded = 1;                                       // mark seeded
        } else {                                                     // no seed
            srand((unsigned int)time(NULL));                         // seed from time
        }
        *result = MAKE_NONE();                                       // return none
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.triangular") == 0) {                                // triangular distribution
        double low = 0.0, high = 1.0, mode = 0.5;                                // defaults
        if (arg_count >= 1) {                                                    // low provided
            if (!IS_NUMBER(args[0])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // validate
            low = AS_NUMBER(args[0]);                                            // set low
        }
        if (arg_count >= 2) {                                                    // high provided
            if (!IS_NUMBER(args[1])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // validate
            high = AS_NUMBER(args[1]);                                           // set high
        }
        if (arg_count >= 3) {                                                    // mode provided
            if (!IS_NUMBER(args[2])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // validate
            mode = AS_NUMBER(args[2]);                                           // set mode
        }
        if (high == low) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NUMBER(low); return true; }  // degenerate case
        double u = (double)rand() / ((double)RAND_MAX + 1.0);                    // uniform
        double cdf_mode = (mode - low) / (high - low);                           // mode cdf
        double result_val;                                                       // result
        if (u < cdf_mode) {                                                      // left side
            result_val = low + sqrt(u * (high - low) * (mode - low));            // inverse cdf left
        } else {                                                                 // right side
            result_val = high - sqrt((1.0 - u) * (high - low) * (high - mode));  // inverse cdf right
        }
        *result = MAKE_NUMBER(result_val);                           // return value
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.expovariate") == 0) {                   // exponential distribution
        if (arg_count != 1 || !IS_NUMBER(args[0])) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; } // validate
        double lambd = AS_NUMBER(args[0]);                           // rate parameter
        if (lambd == 0.0) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // invalid rate
        double u = (double)rand() / ((double)RAND_MAX + 1.0);        // uniform
        if (u < 1e-10) u = 1e-10;                                    // avoid zero
        *result = MAKE_NUMBER(-log(u) / lambd);                      // inverse cdf
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    if (strcmp(name, "random.betavariate") == 0) {                   // beta distribution
        if (arg_count != 2) { APEX_MUTEX_UNLOCK(&g_rng_mutex); *result = MAKE_NONE(); return true; }  // need 2 args
        bool ok1, ok2;                                               // validity flags
        double alpha = get_number_safe(args[0], &ok1);               // alpha parameter
        double beta = get_number_safe(args[1], &ok2);                // beta parameter
        if (!ok1 || !ok2 || alpha <= 0.0 || beta <= 0.0) {           // invalid parameters
            APEX_MUTEX_UNLOCK(&g_rng_mutex);
            *result = MAKE_NONE();
            return true;
        }
        double x = random_gamma(alpha);                              // gamma alpha
        double y = random_gamma(beta);                               // gamma beta
        if (x + y == 0.0) {                                          // both zero
            APEX_MUTEX_UNLOCK(&g_rng_mutex);
            *result = MAKE_NONE();
            return true;
        }
        *result = MAKE_NUMBER(x / (x + y));                          // beta = gamma ratio
        APEX_MUTEX_UNLOCK(&g_rng_mutex);                             // release lock
        return true;                                                 // builtin handled
    }

    APEX_MUTEX_UNLOCK(&g_rng_mutex);   // release lock on the unrecognized-builtin path
    return false;                      // not a recognized builtin
}