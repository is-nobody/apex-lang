// source/libraries/math_module.c
// Implementation of Math Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#define _USE_MATH_DEFINES
#include "math_module.h"
#include "vm.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// dispatcher for mathematical built-in functions
bool math_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    (void)vm;                                                     // vm unused
    
    if (strcmp(name, "math.abs") == 0) {                          // absolute value
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(fabs(AS_NUMBER(args[0])));      // return absolute value
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.floor") == 0) {                        // floor function
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(floor(AS_NUMBER(args[0])));     // return largest integer <= x
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.ceil") == 0) {                         // ceiling function
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(ceil(AS_NUMBER(args[0])));      // return smallest integer >= x
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.round") == 0) {                        // round to decimals
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            double num = AS_NUMBER(args[0]);                      // extract number
            int decimals = 0;                                     // default to 0 decimals
            if (arg_count >= 2 && IS_NUMBER(args[1])) {           // check if decimals provided
                decimals = (int)AS_NUMBER(args[1]);               // extract decimal places
                if (decimals < 0) {                               // negative decimals invalid
                    *result = MAKE_NONE();                        // return none
                    return true;                                  // builtin handled
                }
            }
            double factor = pow(10.0, decimals);                  // scaling factor
            *result = MAKE_NUMBER(round(num * factor) / factor);  // round and scale back
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.sqrt") == 0) {                         // square root
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            double val = AS_NUMBER(args[0]);                      // extract number
            *result = MAKE_NUMBER(sqrt(val));                     // compute square root
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.exp") == 0) {                          // exponential e^x
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(exp(AS_NUMBER(args[0])));       // compute e^x
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.log") == 0) {                          // logarithm
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            double x = AS_NUMBER(args[0]);                        // extract number
            double res;                                           // result storage
            
            if (x < 0) {                                          // negative input
                *result = MAKE_NUMBER(NAN);                       // return nan
                return true;                                      // builtin handled
            }
            if (x == 0) {                                         // zero input
                *result = MAKE_NUMBER(-INFINITY);                 // return -inf
                return true;                                      // builtin handled
            }
            
            if (arg_count >= 2 && IS_NUMBER(args[1])) {           // custom base provided
                double base = AS_NUMBER(args[1]);                 // extract base
                if (base <= 0 || base == 1.0) {                   // invalid base
                    *result = MAKE_NONE();                        // return none
                    return true;                                  // builtin handled
                }
                res = log(x) / log(base);                         // log base change
            } else {                                              // natural log
                res = log(x);                                     // compute ln(x)
            }
            *result = MAKE_NUMBER(res);                           // return result
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.sin") == 0) {                          // sine
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(sin(AS_NUMBER(args[0])));       // compute sine
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.cos") == 0) {                          // cosine
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(cos(AS_NUMBER(args[0])));       // compute cosine
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.tan") == 0) {                          // tangent
        if (arg_count >= 1 && IS_NUMBER(args[0])) {               // validate number argument
            *result = MAKE_NUMBER(tan(AS_NUMBER(args[0])));       // compute tangent
            return true;                                          // builtin handled
        }
        *result = MAKE_NONE();                                    // invalid, return none
        return true;                                              // builtin handled
    }
    
    if (strcmp(name, "math.asin") == 0) {                             // arc sine
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate number argument
            if (AS_NUMBER(args[0]) < -1 || AS_NUMBER(args[0]) > 1) {  // out of domain
                *result = MAKE_NUMBER(NAN);                           // return nan
                return true;                                          // builtin handled
            }
            *result = MAKE_NUMBER(asin(AS_NUMBER(args[0])));          // compute arc sine
            return true;                                              // builtin handled
        }
        *result = MAKE_NONE();                                        // invalid, return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.acos") == 0) {                             // arc cosine
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate number argument
            if (AS_NUMBER(args[0]) < -1 || AS_NUMBER(args[0]) > 1) {  // out of domain
                *result = MAKE_NUMBER(NAN);                           // return nan
                return true;                                          // builtin handled
            }
            *result = MAKE_NUMBER(acos(AS_NUMBER(args[0])));          // compute arc cosine
            return true;                                              // builtin handled
        }
        *result = MAKE_NONE();                                        // invalid, return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.atan") == 0) {                             // arc tangent
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate number argument
            *result = MAKE_NUMBER(atan(AS_NUMBER(args[0])));          // compute arc tangent
            return true;                                              // builtin handled
        }
        *result = MAKE_NONE();                                        // invalid, return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.pi") == 0) {                               // constant pi
        *result = MAKE_NUMBER(M_PI);                                  // return pi
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.e") == 0) {                                // constant e
        *result = MAKE_NUMBER(M_E);                                   // return e
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.inf") == 0) {                              // infinity constant
        *result = MAKE_NUMBER(INFINITY);                              // return infinity
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.isnan") == 0) {                                      // check if nan
        if (arg_count >= 1 && (IS_NUMBER(args[0]) || IS_NAN(args[0]))) {        // validate number argument
            *result = MAKE_BOOL(IS_NAN(args[0]) || isnan(AS_NUMBER(args[0])));  // check for nan
            return true;                                                        // builtin handled
        }
        *result = MAKE_BOOL(false);                                   // invalid, return false
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.isinf") == 0) {                            // check if infinity
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate number argument
            *result = MAKE_BOOL(isinf(AS_NUMBER(args[0])));           // check for infinity
            return true;                                              // builtin handled
        }
        *result = MAKE_BOOL(false);                                   // invalid, return false
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.trunc") == 0) {                            // truncate to integer
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                   // validate number argument
            *result = MAKE_NUMBER(trunc(AS_NUMBER(args[0])));         // truncate toward zero
            return true;                                              // builtin handled
        }
        *result = MAKE_NONE();                                        // invalid, return none
        return true;                                                  // builtin handled
    }
    
    if (strcmp(name, "math.pow") == 0) {                                   // power function
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {  // validate two numbers
            if (AS_NUMBER(args[0]) == 0 && AS_NUMBER(args[1]) == 0) {      // 0^0 special case
                *result = MAKE_NUMBER(1.0);                                // return 1
                return true;                                               // builtin handled
            }
            *result = MAKE_NUMBER(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));  // compute power
            return true;                                                         // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.atan2") == 0) {                                         // arc tangent of y/x
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {          // validate two numbers
            *result = MAKE_NUMBER(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1])));  // compute atan2
            return true;                                                           // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.radians") == 0) {                               // degrees to radians
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                        // validate number argument
            *result = MAKE_NUMBER(AS_NUMBER(args[0]) * M_PI / 180.0);      // convert to radians
            return true;                                                   // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.degrees") == 0) {                               // radians to degrees
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                        // validate number argument
            *result = MAKE_NUMBER(AS_NUMBER(args[0]) * 180.0 / M_PI);      // convert to degrees
            return true;                                                   // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.hypot") == 0) {                                         // euclidean distance
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {          // validate two numbers
            *result = MAKE_NUMBER(hypot(AS_NUMBER(args[0]), AS_NUMBER(args[1])));  // compute hypotenuse
            return true;                                                           // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.gcd") == 0) {                                   // greatest common divisor
        if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {  // validate two numbers
            long a = (long)fabs(AS_NUMBER(args[0]));                       // absolute value first
            long b = (long)fabs(AS_NUMBER(args[1]));                       // absolute value second
            while (b != 0) {                                               // euclidean algorithm
                long t = b;                                                // store b
                b = a % b;                                                 // compute remainder
                a = t;                                                     // swap
            }
            *result = MAKE_NUMBER((double)a);                              // return gcd
            return true;                                                   // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    if (strcmp(name, "math.factorial") == 0) {                             // factorial
        if (arg_count >= 1 && IS_NUMBER(args[0])) {                        // validate number argument
            double n = AS_NUMBER(args[0]);                                 // extract number
            if (n < 0 || n != floor(n)) {                                  // negative or non-integer
                *result = MAKE_NUMBER(NAN);                                // return nan
                return true;                                               // builtin handled
            }
            if (n > 170) {                                                 // overflow beyond max
                *result = MAKE_NUMBER(INFINITY);                           // return infinity
                return true;                                               // builtin handled
            }
            double res = 1.0;                                              // result accumulator
            for (double i = 2.0; i <= n; i++) {                            // iterative multiplication
                res *= i;                                                  // multiply
            }
            *result = MAKE_NUMBER(res);                                    // return factorial
            return true;                                                   // builtin handled
        }
        *result = MAKE_NONE();                                             // invalid, return none
        return true;                                                       // builtin handled
    }
    
    return false;                                                          // not a recognized builtin
}