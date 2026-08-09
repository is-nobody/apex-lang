// source/tests/arithmetic/whole_with_decimal.apex
// Implementation of Whole with Decimal Arithmetic Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: addition of whole and decimal number
function whole_with_decimal_add()
    result = 5 + 2.5
    assert_equal(result, 7.5, "5 + 2.5")

// test: subtraction of decimal from whole number
function whole_with_decimal_subtract()
    result = 5 - 2.5
    assert_equal(result, 2.5, "5 - 2.5")

// test: division of whole by decimal, checks floating-point result
function whole_with_decimal_divide()
    result = 5 / 2.5
    assert_equal(result, 2.0, "5 / 2.5")

// test: multiplication of whole with decimal yields decimal product
function whole_with_decimal_multiply()
    result = 5 * 2.5
    assert_equal(result, 12.5, "5 * 2.5")

// test: modulo with decimal divisor, whole number divisible case
function whole_with_decimal_modulo()
    result = 5 % 2.5
    assert_equal(result, 0, "5 % 2.5")

// execute all mixed-type arithmetic test cases
whole_with_decimal_add()
whole_with_decimal_subtract()
whole_with_decimal_divide()
whole_with_decimal_multiply()
whole_with_decimal_modulo()

// report completion of whole with decimal tests
os.output("[PASS] arithmetic/whole_with_decimal.apex")