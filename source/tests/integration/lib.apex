// source/tests/integration/lib.apex
// Implementation of Core Library Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import math

// module-level constants
pi = math.pi()
e = math.e()

// basic arithmetic: addition
function add(a, b)
    return a + b

// basic arithmetic: subtraction
function subtract(a, b)
    return a - b

// basic arithmetic: multiplication
function multiply(a, b)
    return a * b

// basic arithmetic: division with zero check
function divide(a, b)
    if b == 0
        os.output("Error: Division by zero")
        return 0
    return a / b

// math: integer exponentiation (iterative)
function power(base, exp)
    result = 1
    i = 0
    for i = 1, exp
        result = result * base
    return result

// math: recursive factorial
function factorial(n)
    if n <= 1
        return 1
    return n * factorial(n - 1)

// math: circle area using module pi constant
function circle_area(radius)
    return pi * radius * radius