#define _USE_MATH_DEFINES
#include "math_module.h"
#include "vm.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// dispatcher for mathematical built-in functions
bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;
    
    if (strcmp(name, "math.abs") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(fabs(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.floor") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(floor(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.ceil") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(ceil(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.round") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            double num = AS_NUMBER(args[0]);
            int decimals = 0;
            if (arg_count >= 2 && IS_NUMBER(args[1])) {
                decimals = (int)AS_NUMBER(args[1]);
                if (decimals < 0) {
                    *result = MAKE_NONE();
                    return true;
                }
            }
            double factor = pow(10.0, decimals);
            *result = MAKE_NUMBER(round(num * factor) / factor);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.sqrt") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            double val = AS_NUMBER(args[0]);
            *result = MAKE_NUMBER(sqrt(val));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.exp") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(exp(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.log") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            double x = AS_NUMBER(args[0]);
            double res;
            
            if (x < 0) {
                *result = MAKE_NUMBER(NAN);
                return true;
            }
            if (x == 0) {
                *result = MAKE_NUMBER(-INFINITY);
                return true;
            }
            
            if (arg_count >= 2 && IS_NUMBER(args[1])) {
                double base = AS_NUMBER(args[1]);
                if (base <= 0 || base == 1.0) {
                    *result = MAKE_NONE();
                    return true;
                }
                res = log(x) / log(base);
            } else {
                res = log(x);
            }
            *result = MAKE_NUMBER(res);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.sin") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(sin(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.cos") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(cos(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.tan") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(tan(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.asin") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            if (AS_NUMBER(args[0]) < -1 || AS_NUMBER(args[0]) > 1) {
                *result = MAKE_NUMBER(NAN);
                return true;
            }
            *result = MAKE_NUMBER(asin(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.acos") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            if (AS_NUMBER(args[0]) < -1 || AS_NUMBER(args[0]) > 1) {
                *result = MAKE_NUMBER(NAN);
                return true;
            }
            *result = MAKE_NUMBER(acos(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.atan") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(atan(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.pi") == 0) {
        *result = MAKE_NUMBER(M_PI);
        return true;
    }
    
    if (strcmp(name, "math.e") == 0) {
        *result = MAKE_NUMBER(M_E);
        return true;
    }
    
    if (strcmp(name, "math.inf") == 0) {
        *result = MAKE_NUMBER(INFINITY);
        return true;
    }
    
    if (strcmp(name, "math.isnan") == 0) {
        if (arg_count >= 1 && (IS_NUMBER(args[0]) || IS_NAN(args[0]))) {
            *result = MAKE_BOOL(IS_NAN(args[0]) || isnan(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.isinf") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_BOOL(isinf(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.trunc") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(trunc(AS_NUMBER(args[0])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.pow") == 0) {
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
            if (AS_NUMBER(args[0]) == 0 && AS_NUMBER(args[1]) == 0) {
                *result = MAKE_NUMBER(1.0);
                return true;
            }
            *result = MAKE_NUMBER(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.atan2") == 0) {
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
            *result = MAKE_NUMBER(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.radians") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(AS_NUMBER(args[0]) * M_PI / 180.0);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.degrees") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            *result = MAKE_NUMBER(AS_NUMBER(args[0]) * 180.0 / M_PI);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.hypot") == 0) {
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
            *result = MAKE_NUMBER(hypot(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.gcd") == 0) {
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
            long a = (long)fabs(AS_NUMBER(args[0]));
            long b = (long)fabs(AS_NUMBER(args[1]));
            while (b != 0) {
                long t = b;
                b = a % b;
                a = t;
            }
            *result = MAKE_NUMBER((double)a);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    if (strcmp(name, "math.factorial") == 0) {
        if (arg_count >= 1 && IS_NUMBER(args[0])) {
            double n = AS_NUMBER(args[0]);
            if (n < 0 || n != floor(n)) {
                *result = MAKE_NUMBER(NAN);
                return true;
            }
            if (n > 170) {
                *result = MAKE_NUMBER(INFINITY);
                return true;
            }
            double res = 1.0;
            for (double i = 2.0; i <= n; i++) {
                res *= i;
            }
            *result = MAKE_NUMBER(res);
            return true;
        }
        *result = MAKE_NONE();
        return true;
    }
    
    return false;
}