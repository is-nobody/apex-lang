// source/tests/arithmetic/decimal_with_decimal.apex
// Implementation of Decimal Number Arithmetic Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and logs a formatted error if they differ
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: addition of two decimal literals
function decimal_with_decimal_add()
    result = 3.5 + 1.5
    assert_equal(result, 5, "3.5 + 1.5")

// test: subtraction of two decimal literals
function decimal_with_decimal_subtract()
    result = 3.5 - 1.5
    assert_equal(result, 2, "3.5 - 1.5")

// test: division of two decimal literals, verifies floating-point precision
function decimal_with_decimal_divide()
    result = 3.5 / 2.8
    assert_equal(result, 1.25, "3.5 / 2.8")

// test: multiplication of two decimal literals
function decimal_with_decimal_multiply()
    result = 3.5 * 1.5
    assert_equal(result, 5.25, "3.5 * 1.5")

// test: modulo operation on two decimal literals (remainder)
function decimal_with_decimal_modulo()
    result = 3.5 % 1.5
    assert_equal(result, 0.5, "3.5 % 1.5")

// execute all test cases
decimal_with_decimal_add()
decimal_with_decimal_subtract()
decimal_with_decimal_divide()
decimal_with_decimal_multiply()
decimal_with_decimal_modulo()

// report completion of the test file
os.output("[PASS] arithmetic/decimal_with_decimal.apex")