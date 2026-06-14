#include "math_module.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "math.abs") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(fabs(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.floor") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(floor(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.ceil") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(ceil(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.round") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double num = args[0].number;
            int decimals = 0;
            if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
                decimals = (int)args[1].number;
            }
            double factor = pow(10.0, decimals);
            *result = vm_make_number(round(num * factor) / factor);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.sqrt") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            // Let C library handle domain errors (returns NaN for negative)
            *result = vm_make_number(sqrt(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.exp") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(exp(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.log") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            double res;
            
            if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
                double base = args[1].number;
                if (base <= 0 || base == 1.0) {
                    *result = vm_make_number(NAN);
                    return true;
                }
                res = log(x) / log(base);
            } else {
                // Let C library handle domain errors (returns -inf for 0, nan for negative)
                res = log(x);
            }
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.sin") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(sin(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.cos") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(cos(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.tan") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(tan(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.asin") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(asin(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.acos") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(acos(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.atan") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(atan(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.pi") == 0) {
        *result = vm_make_number(M_PI);
        return true;
    }
    if (strcmp(name, "math.e") == 0) {
        *result = vm_make_number(M_E);
        return true;
    }
    if (strcmp(name, "math.inf") == 0) {
        *result = vm_make_number(INFINITY);
        return true;
    }
    if (strcmp(name, "math.isnan") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_bool(isnan(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.isinf") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_bool(isinf(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.trunc") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(trunc(args[0].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.pow") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_NUMBER && args[1].type == VAL_NUMBER) {
            *result = vm_make_number(pow(args[0].number, args[1].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.atan2") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_NUMBER && args[1].type == VAL_NUMBER) {
            *result = vm_make_number(atan2(args[0].number, args[1].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.radians") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(args[0].number * M_PI / 180.0);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.degrees") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(args[0].number * 180.0 / M_PI);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.hypot") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_NUMBER && args[1].type == VAL_NUMBER) {
            *result = vm_make_number(hypot(args[0].number, args[1].number));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.gcd") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_NUMBER && args[1].type == VAL_NUMBER) {
            long a = (long)fabs(args[0].number);
            long b = (long)fabs(args[1].number);
            while (b != 0) {
                long t = b;
                b = a % b;
                a = t;
            }
            *result = vm_make_number((double)a);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.factorial") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double n = args[0].number;
            if (n < 0 || n != floor(n)) return false;
            if (n > 170) return false;
            double res = 1.0;
            for (double i = 2.0; i <= n; i++) {
                res *= i;
            }
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    return false;
}