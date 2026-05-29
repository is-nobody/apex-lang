#include "math_module.h"
#include <math.h>
#include <string.h>

bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    if (strcmp(name, "math.abs") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(fabs(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.floor") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(floor(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.ceil") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(ceil(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.round") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double num = args[0].number;
            int decimals = 0;
            if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
                decimals = (int)args[1].number;
            }
            // Round to specified number of decimal places
            double factor = pow(10.0, decimals);
            *result = vm_make_number(round(num * factor) / factor);
        }
        return true;
    }
    
    if (strcmp(name, "math.sqrt") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(sqrt(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.exp") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(exp(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.log") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            double x = args[0].number;
            if (arg_count >= 2 && args[1].type == VAL_NUMBER) {
                // log with custom base: log_b(x) = ln(x) / ln(b)
                double base = args[1].number;
                *result = vm_make_number(log(x) / log(base));
            } else {
                // Natural logarithm
                *result = vm_make_number(log(x));
            }
        }
        return true;
    }
    
    if (strcmp(name, "math.sin") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(sin(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.cos") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(cos(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.tan") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(tan(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.asin") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(asin(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.acos") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(acos(args[0].number));
        }
        return true;
    }
    
    if (strcmp(name, "math.atan") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_NUMBER) {
            *result = vm_make_number(atan(args[0].number));
        }
        return true;
    }
    
    // Unknown function name
    return false;
}