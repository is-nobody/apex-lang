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
    // abs of -infinity must still be infinite
    assert_equal(math.is_inf(math.abs(-math.inf())), true, "math.abs(-inf) is inf")
    // abs of NaN stays NaN
    assert_equal(math.is_nan(math.abs(math.sqrt(-1))), true, "math.abs(nan) is nan")

// test: arc cosine (inverse cosine)
function acos()
    assert_float_equal(math.acos(0.5), 1.0471975511966, "math.acos(0.5)")
    // argument outside [-1, 1] -> NaN
    assert_equal(math.is_nan(math.acos(2)), true, "math.acos(2) is nan")
    assert_equal(math.is_nan(math.acos(-2)), true, "math.acos(-2) is nan")

// test: arc sine (inverse sine)
function asin()
    assert_float_equal(math.asin(0.5), 0.523598775598299, "math.asin(0.5)")
    // argument outside [-1, 1] -> NaN
    assert_equal(math.is_nan(math.asin(2)), true, "math.asin(2) is nan")
    assert_equal(math.is_nan(math.asin(-2)), true, "math.asin(-2) is nan")

// test: arc tangent (inverse tangent)
function atan()
    assert_float_equal(math.atan(1), 0.785398163397448, "math.atan(1)")
    // atan(+/-inf) is +/-pi/2, both finite
    assert_float_equal(math.atan(math.inf()), math.pi() / 2, "math.atan(inf)")
    assert_float_equal(math.atan(-math.inf()), -math.pi() / 2, "math.atan(-inf)")
    assert_equal(math.is_nan(math.atan(math.sqrt(-1))), true, "math.atan(nan) is nan")

// test: arc tangent with two arguments (quadrant-aware)
function atan2()
    assert_float_equal(math.atan2(1, 1), 0.785398163397448, "math.atan2(1, 1)")
    // atan2(0, 0) is implementation-defined but never infinite
    assert_equal(math.is_inf(math.atan2(0, 0)), false, "math.atan2(0, 0) is finite")
    assert_equal(math.is_nan(math.atan2(0, 0)), false, "math.atan2(0, 0) is not nan")
    // atan2 with an inf coordinate is well-defined and finite
    assert_float_equal(math.atan2(math.inf(), 1), math.pi() / 2, "math.atan2(inf, 1)")

// test: round up to nearest integer
function round_up()
    assert_equal(math.round_up(3.7), 4, "math.round_up(3.7)")
    assert_equal(math.round_up(-3.7), -3, "math.round_up(-3.7)")
    assert_equal(math.round_up(3.2), 4, "math.round_up(3.2)")
    // ceil of infinity stays infinity
    assert_equal(math.is_inf(math.round_up(math.inf())), true, "math.round_up(inf) is inf")
    // ceil of NaN stays NaN
    assert_equal(math.is_nan(math.round_up(math.sqrt(-1))), true, "math.round_up(nan) is nan")

// test: cosine of angle in radians
function cos()
    result = math.cos(60 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.cos(60 deg)")
    // cos(+/-inf) is NaN
    assert_equal(math.is_nan(math.cos(math.inf())), true, "math.cos(inf) is nan")
    assert_equal(math.is_nan(math.cos(-math.inf())), true, "math.cos(-inf) is nan")

// test: radians to degrees conversion
function degrees()
    assert_float_equal(math.degrees(math.pi()), 180, "math.degrees(pi)")
    // degrees of inf -> inf
    assert_equal(math.is_inf(math.degrees(math.inf())), true, "math.degrees(inf) is inf")
    // degrees of NaN -> NaN
    assert_equal(math.is_nan(math.degrees(math.sqrt(-1))), true, "math.degrees(nan) is nan")

// test: Euler's number constant
function e()
    assert_float_equal(math.e(), 2.71828182845905, "math.e()")

// test: exponential function e^x
function exponent()
    assert_float_equal(math.exponent(1), 2.71828182845905, "math.exponent(1)")
    assert_equal(math.exponent(0), 1, "math.exponent(0)")
    // e^x overflows to +inf for large x
    assert_equal(math.is_inf(math.exponent(1000)), true, "math.exponent(1000) is inf")
    // e^x underflows to 0 (finite) for large negative x
    assert_equal(math.is_inf(math.exponent(-1000)), false, "math.exponent(-1000) is finite")
    // e^NaN is NaN
    assert_equal(math.is_nan(math.exponent(math.sqrt(-1))), true, "math.exponent(nan) is nan")

// test: factorial of integer
function factorial()
    assert_equal(math.factorial(5), 120, "math.factorial(5)")
    assert_equal(math.factorial(0), 1, "math.factorial(0)")
    assert_equal(type(math.factorial(171)), "number", "math.factorial(171)")
    // 171! overflows double precision -> +inf
    assert_equal(math.is_inf(math.factorial(171)), true, "math.factorial(171) is inf")
    assert_equal(math.is_inf(math.factorial(1000)), true, "math.factorial(1000) is inf")
    // 170! is the largest finite factorial representable as a double
    assert_equal(math.is_inf(math.factorial(170)), false, "math.factorial(170) is finite")
    // factorial is not defined for negative integers -> NaN (or error); guard against NaN
    assert_equal(math.is_nan(math.factorial(-1)), true, "math.factorial(-1) is nan")

// test: round down to nearest integer
function round_down()
    assert_equal(math.round_down(3.7), 3, "math.round_down(3.7)")
    assert_equal(math.round_down(-3.7), -4, "math.round_down(-3.7)")
    assert_equal(math.round_down(3.2), 3, "math.round_down(3.2)")
    // floor of infinity stays infinity
    assert_equal(math.is_inf(math.round_down(math.inf())), true, "math.round_down(inf) is inf")
    // floor of NaN stays NaN
    assert_equal(math.is_nan(math.round_down(math.sqrt(-1))), true, "math.round_down(nan) is nan")

// test: greatest common divisor of two integers
function gcd()
    assert_equal(math.gcd(12, 8), 4, "math.gcd(12, 8)")
    assert_equal(math.gcd(54, 24), 6, "math.gcd(54, 24)")
    // gcd must never produce non-finite values for finite inputs
    assert_equal(math.is_inf(math.gcd(0, 0)), false, "math.gcd(0, 0) is finite")
    assert_equal(math.is_nan(math.gcd(0, 0)), false, "math.gcd(0, 0) is not nan")

// test: Euclidean norm (hypotenuse)
function hypot()
    assert_equal(math.hypotenuse(3, 4), 5, "math.hypotenuse(3, 4)")
    // hypot(inf, x) is +inf
    assert_equal(math.is_inf(math.hypotenuse(math.inf(), 1)), true, "math.hypotenuse(inf, 1) is inf")
    // hypot(NaN, x) is NaN
    assert_equal(math.is_nan(math.hypotenuse(math.sqrt(-1), 1)), true, "math.hypotenuse(nan, 1) is nan")

// test: positive infinity constant
function inf()
    result = math.inf()
    isinf = math.is_inf(result)
    assert_equal(isinf, true, "math.inf() is infinite")
    // +inf is not NaN
    assert_equal(math.is_nan(math.inf()), false, "math.inf() is not nan")

// test: check if value is infinite
function is_inf()
    assert_equal(math.is_inf(math.inf()), true, "math.is_inf(inf)")
    assert_equal(math.is_inf(1), false, "math.is_inf(1)")
    assert_equal(math.is_inf(-math.inf()), true, "math.is_inf(-inf)")
    assert_equal(math.is_inf(math.sqrt(-1)), false, "math.is_inf(nan) is false")

// test: check if value is NaN (not a number)
function is_nan()
    assert_equal(math.is_nan(math.sqrt(-1)), true, "math.is_nan(sqrt(-1))")
    assert_equal(math.is_nan(1), false, "math.is_nan(1)")
    assert_equal(math.is_nan(math.inf()), false, "math.is_nan(inf) is false")
    assert_equal(math.is_nan(-math.inf()), false, "math.is_nan(-inf) is false")

// test: natural logarithm and base-N logarithm
function log()
    assert_float_equal(math.log(2.71828), 0.999999327347282, "math.log(2.71828)")
    assert_equal(math.log(100, 10), 2, "math.log(100, 10)")
    // log(0) is -infinity
    assert_equal(math.is_inf(math.log(0)), true, "math.log(0) is inf")
    // log of a negative number is NaN
    assert_equal(math.is_nan(math.log(-1)), true, "math.log(-1) is nan")
    // log(-1, 10) is NaN
    assert_equal(math.is_nan(math.log(-1, 10)), true, "math.log(-1, 10) is nan")

// test: pi constant
function pi()
    assert_float_equal(math.pi(), 3.14159265358979, "math.pi()")

// test: power function (base^exponent)
function power()
    assert_equal(math.power(2, 10), 1024, "math.power(2, 10)")
    assert_equal(math.power(9, 0.5), 3, "math.power(9, 0.5)")
    // overflow -> +inf
    assert_equal(math.is_inf(math.power(10, 1000)), true, "math.power(10, 1000) is inf")
    // 0^negative -> +inf
    assert_equal(math.is_inf(math.power(0, -1)), true, "math.power(0, -1) is inf")
    // negative base with non-integer exponent -> NaN
    assert_equal(math.is_nan(math.power(-1, 0.5)), true, "math.power(-1, 0.5) is nan")

// test: degrees to radians conversion
function radians()
    assert_float_equal(math.radians(180), math.pi(), "math.radians(180)")
    // radians of inf -> inf
    assert_equal(math.is_inf(math.radians(math.inf())), true, "math.radians(inf) is inf")
    // radians of NaN -> NaN
    assert_equal(math.is_nan(math.radians(math.sqrt(-1))), true, "math.radians(nan) is nan")

// test: round to nearest integer or specified precision
function round()
    assert_equal(math.round(3.14159), 3, "math.round(3.14159)")
    assert_equal(math.round(3.14159, 2), 3.14, "math.round(3.14159, 2)")
    assert_equal(math.round(3.14159, 4), 3.1416, "math.round(3.14159, 4)")
    // round(inf) stays inf
    assert_equal(math.is_inf(math.round(math.inf())), true, "math.round(inf) is inf")
    // round(nan) stays nan
    assert_equal(math.is_nan(math.round(math.sqrt(-1))), true, "math.round(nan) is nan")

// test: sine of angle in radians
function sin()
    result = math.sin(30 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.sin(30 deg)")
    // sin(+/-inf) is NaN
    assert_equal(math.is_nan(math.sin(math.inf())), true, "math.sin(inf) is nan")
    assert_equal(math.is_nan(math.sin(-math.inf())), true, "math.sin(-inf) is nan")

// test: square root
function sqrt()
    assert_equal(math.sqrt(16), 4, "math.sqrt(16)")
    assert_float_equal(math.sqrt(2), 1.4142135623731, "math.sqrt(2)")
    assert_equal(math.sqrt(0), 0, "math.sqrt(0)")
    // sqrt of a negative number is NaN
    assert_equal(math.is_nan(math.sqrt(-1)), true, "math.sqrt(-1) is nan")
    // sqrt(+inf) is +inf
    assert_equal(math.is_inf(math.sqrt(math.inf())), true, "math.sqrt(inf) is inf")
    // sqrt(nan) is nan
    assert_equal(math.is_nan(math.sqrt(math.sqrt(-1))), true, "math.sqrt(nan) is nan")

// test: tangent of angle in radians
function tan()
    result = math.tan(45 * math.pi() / 180)
    assert_float_equal(result, 1, "math.tan(45 deg)")
    // tan(+/-inf) is NaN
    assert_equal(math.is_nan(math.tan(math.inf())), true, "math.tan(inf) is nan")
    assert_equal(math.is_nan(math.tan(-math.inf())), true, "math.tan(-inf) is nan")

// test: truncate decimal (remove fractional part)
function drop_decimal()
    assert_equal(math.drop_decimal(3.9), 3, "math.drop_decimal(3.9)")
    assert_equal(math.drop_decimal(-3.9), -3, "math.drop_decimal(-3.9)")
    // trunc(inf) stays inf
    assert_equal(math.is_inf(math.drop_decimal(math.inf())), true, "math.drop_decimal(inf) is inf")
    // trunc(nan) stays nan
    assert_equal(math.is_nan(math.drop_decimal(math.sqrt(-1))), true, "math.drop_decimal(nan) is nan")

// execute all math library test cases
abs()
acos()
asin()
atan()
atan2()
round_up()
cos()
degrees()
e()
exponent()
factorial()
round_down()
gcd()
hypot()
inf()
is_inf()
is_nan()
log()
pi()
power()
radians()
round()
sin()
sqrt()
tan()
drop_decimal()

// report completion of math library tests
os.output("[PASS] libraries/math.apex")