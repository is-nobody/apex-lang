// source/tests/libraries/math.apex
// Implementation of Math Library Function Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import math

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: compares floating point values with tolerance
function assert_float_equal(actual, expected, name)
    diff = actual - expected
    if diff < 0
        diff = -diff
    if diff >= 0.00000000000001
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: absolute value of integer and float
function abs()
    assert_equal(math.abs(-5), 5, "math.abs(-5)")
    assert_equal(math.abs(3.14), 3.14, "math.abs(3.14)")

// test: arc cosine (inverse cosine)
function acos()
    assert_float_equal(math.acos(0.5), 1.0471975511966, "math.acos(0.5)")

// test: arc sine (inverse sine)
function asin()
    assert_float_equal(math.asin(0.5), 0.523598775598299, "math.asin(0.5)")

// test: arc tangent (inverse tangent)
function atan()
    assert_float_equal(math.atan(1), 0.785398163397448, "math.atan(1)")

// test: arc tangent with two arguments (quadrant-aware)
function atan2()
    assert_float_equal(math.atan2(1, 1), 0.785398163397448, "math.atan2(1, 1)")

// test: ceiling (round up to nearest integer)
function ceil()
    assert_equal(math.round_up(3.7), 4, "math.round_up(3.7)")
    assert_equal(math.round_up(-3.7), -3, "math.round_up(-3.7)")
    assert_equal(math.round_up(3.2), 4, "math.round_up(3.2)")

// test: cosine of angle in radians
function cos()
    result = math.cos(60 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.cos(60 deg)")

// test: radians to degrees conversion
function degrees()
    assert_float_equal(math.degrees(math.pi()), 180, "math.degrees(pi)")

// test: Euler's number constant
function e()
    assert_float_equal(math.e(), 2.71828182845905, "math.e()")

// test: exponential function e^x
function exp()
    assert_float_equal(math.exp(1), 2.71828182845905, "math.exp(1)")
    assert_equal(math.exp(0), 1, "math.exp(0)")

// test: factorial of integer
function factorial()
    assert_equal(math.factorial(5), 120, "math.factorial(5)")
    assert_equal(math.factorial(0), 1, "math.factorial(0)")

// test: floor (round down to nearest integer)
function floor()
    assert_equal(math.round_down(3.7), 3, "math.round_down(3.7)")
    assert_equal(math.round_down(-3.7), -4, "math.round_down(-3.7)")
    assert_equal(math.round_down(3.2), 3, "math.round_down(3.2)")

// test: greatest common divisor of two integers
function gcd()
    assert_equal(math.gcd(12, 8), 4, "math.gcd(12, 8)")
    assert_equal(math.gcd(54, 24), 6, "math.gcd(54, 24)")

// test: Euclidean norm (hypotenuse)
function hypot()
    assert_equal(math.hypotenuse(3, 4), 5, "math.hypotenuse(3, 4)")

// test: positive infinity constant
function inf()
    result = math.inf()
    is_inf = math.is_inf(result)
    assert_equal(is_inf, true, "math.inf() is infinite")

// test: check if value is infinite
function isinf()
    assert_equal(math.is_inf(math.inf()), true, "math.is_inf(inf)")
    assert_equal(math.is_inf(1), false, "math.is_inf(1)")

// test: check if value is NaN (not a number)
function isnan()
    assert_equal(math.is_nan(math.sqrt(-1)), true, "math.is_nan(sqrt(-1))")
    assert_equal(math.is_nan(1), false, "math.is_nan(1)")

// test: natural logarithm and base-N logarithm
function log()
    assert_float_equal(math.log(2.71828), 0.999999327347282, "math.log(2.71828)")
    assert_equal(math.log(100, 10), 2, "math.log(100, 10)")

// test: pi constant
function pi()
    assert_float_equal(math.pi(), 3.14159265358979, "math.pi()")

// test: power function (base^exponent)
function pow()
    assert_equal(math.pow(2, 10), 1024, "math.pow(2, 10)")
    assert_equal(math.pow(9, 0.5), 3, "math.pow(9, 0.5)")

// test: degrees to radians conversion
function radians()
    assert_float_equal(math.radians(180), math.pi(), "math.radians(180)")

// test: round to nearest integer or specified precision
function round()
    assert_equal(math.round(3.14159), 3, "math.round(3.14159)")
    assert_equal(math.round(3.14159, 2), 3.14, "math.round(3.14159, 2)")
    assert_equal(math.round(3.14159, 4), 3.1416, "math.round(3.14159, 4)")

// test: sine of angle in radians
function sin()
    result = math.sin(30 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.sin(30 deg)")

// test: square root
function sqrt()
    assert_equal(math.sqrt(16), 4, "math.sqrt(16)")
    assert_float_equal(math.sqrt(2), 1.4142135623731, "math.sqrt(2)")
    assert_equal(math.sqrt(0), 0, "math.sqrt(0)")

// test: tangent of angle in radians
function tan()
    result = math.tan(45 * math.pi() / 180)
    assert_float_equal(result, 1, "math.tan(45 deg)")

// test: truncate decimal (remove fractional part)
function trunc()
    assert_equal(math.drop_decimal(3.9), 3, "math.drop_decimal(3.9)")
    assert_equal(math.drop_decimal(-3.9), -3, "math.drop_decimal(-3.9)")

// execute all math library test cases
abs()
acos()
asin()
atan()
atan2()
ceil()
cos()
degrees()
e()
exp()
factorial()
floor()
gcd()
hypot()
inf()
isinf()
isnan()
log()
pi()
pow()
radians()
round()
sin()
sqrt()
tan()
trunc()

// report completion of math library tests
os.output("[PASS] libraries/math.apex")