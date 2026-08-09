// source/tests/integration/lib/subfolder/math_utils.apex
// Implementation of Mathematical Utility Functions for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import math

// helper: calculates area of a circle given radius
function circle_area(radius)
    return math.pi() * radius * radius

// helper: raises base to the exponent power (integer exponent)
function power(base, exp)
    result = 1
    i = 0
    for i = 1, exp
        result = result * base
    return result

// helper: computes factorial of a non-negative integer
function factorial(n)
    result = 1
    for i = 2, n, 1
        result = result * i
    return result

// helper: returns absolute value of a number
function absolute(x)
    if x < 0
        return -x
    return x

// helper: checks if a number is even
function is_even(n)
    return n % 2 == 0

// helper: checks if a number is odd
function is_odd(n)
    return n % 2 != 0