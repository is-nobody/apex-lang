// math_combined.apex
import os
import math

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function assert_float_equal(actual, expected, name)
    diff = actual - expected
    if diff < 0
        diff = -diff
    if diff < 0.00000000000001
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function abs()
    assert_equal(math.abs(-5), 5, "math.abs(-5)")
    assert_equal(math.abs(3.14), 3.14, "math.abs(3.14)")

function acos()
    assert_float_equal(math.acos(0.5), 1.0471975511966, "math.acos(0.5)")

function asin()
    assert_float_equal(math.asin(0.5), 0.523598775598299, "math.asin(0.5)")

function atan()
    assert_float_equal(math.atan(1), 0.785398163397448, "math.atan(1)")

function atan2()
    assert_float_equal(math.atan2(1, 1), 0.785398163397448, "math.atan2(1, 1)")

function ceil()
    assert_equal(math.ceil(3.7), 4, "math.ceil(3.7)")
    assert_equal(math.ceil(-3.7), -3, "math.ceil(-3.7)")
    assert_equal(math.ceil(3.2), 4, "math.ceil(3.2)")

function cos()
    result = math.cos(60 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.cos(60 deg)")

function degrees()
    assert_float_equal(math.degrees(math.pi()), 180, "math.degrees(pi)")

function e()
    assert_float_equal(math.e(), 2.71828182845905, "math.e()")

function exp()
    assert_float_equal(math.exp(1), 2.71828182845905, "math.exp(1)")
    assert_equal(math.exp(0), 1, "math.exp(0)")

function factorial()
    assert_equal(math.factorial(5), 120, "math.factorial(5)")
    assert_equal(math.factorial(0), 1, "math.factorial(0)")

function floor()
    assert_equal(math.floor(3.7), 3, "math.floor(3.7)")
    assert_equal(math.floor(-3.7), -4, "math.floor(-3.7)")
    assert_equal(math.floor(3.2), 3, "math.floor(3.2)")

function gcd()
    assert_equal(math.gcd(12, 8), 4, "math.gcd(12, 8)")
    assert_equal(math.gcd(54, 24), 6, "math.gcd(54, 24)")

function hypot()
    assert_equal(math.hypot(3, 4), 5, "math.hypot(3, 4)")

function inf()
    result = math.inf()
    is_inf = math.isinf(result)
    assert_equal(is_inf, true, "math.inf() is infinite")

function isinf()
    assert_equal(math.isinf(math.inf()), true, "math.isinf(inf)")
    assert_equal(math.isinf(1), false, "math.isinf(1)")

function isnan()
    assert_equal(math.isnan(math.sqrt(-1)), true, "math.isnan(sqrt(-1))")
    assert_equal(math.isnan(1), false, "math.isnan(1)")

function log()
    assert_float_equal(math.log(2.71828), 0.999999327347282, "math.log(2.71828)")
    assert_equal(math.log(100, 10), 2, "math.log(100, 10)")

function pi()
    assert_float_equal(math.pi(), 3.14159265358979, "math.pi()")

function pow()
    assert_equal(math.pow(2, 10), 1024, "math.pow(2, 10)")
    assert_equal(math.pow(9, 0.5), 3, "math.pow(9, 0.5)")

function radians()
    assert_float_equal(math.radians(180), math.pi(), "math.radians(180)")

function round()
    assert_equal(math.round(3.14159), 3, "math.round(3.14159)")
    assert_equal(math.round(3.14159, 2), 3.14, "math.round(3.14159, 2)")
    assert_equal(math.round(3.14159, 4), 3.1416, "math.round(3.14159, 4)")

function sin()
    result = math.sin(30 * math.pi() / 180)
    assert_float_equal(result, 0.5, "math.sin(30 deg)")

function sqrt()
    assert_equal(math.sqrt(16), 4, "math.sqrt(16)")
    assert_float_equal(math.sqrt(2), 1.4142135623731, "math.sqrt(2)")
    assert_equal(math.sqrt(0), 0, "math.sqrt(0)")

function tan()
    result = math.tan(45 * math.pi() / 180)
    assert_float_equal(result, 1, "math.tan(45 deg)")

function trunc()
    assert_equal(math.trunc(3.9), 3, "math.trunc(3.9)")
    assert_equal(math.trunc(-3.9), -3, "math.trunc(-3.9)")

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

if tests_failed == 0
    os.output("[OK] libraries/math.apex")