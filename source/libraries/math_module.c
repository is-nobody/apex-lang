#include "math_module.h"
#include <math.h>
#include <string.h>

static bool is_valid_number(double val) {
    return !isnan(val) && !isinf(val);
}

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
            double res = round(num * factor) / factor;
            if (!is_valid_number(res)) return false;
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.sqrt") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            if (x < 0) return false;
            double res = sqrt(x);
            if (!is_valid_number(res)) return false;
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.exp") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double res = exp(args[0].number);
            if (!is_valid_number(res)) return false;
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.log") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            if (x <= 0) return false;
            double res;
            if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
                double base = args[1].number;
                if (base <= 0 || base == 1.0) return false;
                res = log(x) / log(base);
            } else {
                res = log(x);
            }
            if (!is_valid_number(res)) return false;
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
            double res = tan(args[0].number);
            if (!is_valid_number(res)) return false;
            *result = vm_make_number(res);
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.asin") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            if (x < -1.0 || x > 1.0) return false;
            *result = vm_make_number(asin(x));
            return true;
        }
        return false;
    }
    if (strcmp(name, "math.acos") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            if (x < -1.0 || x > 1.0) return false;
            *result = vm_make_number(acos(x));
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
    return false;
}