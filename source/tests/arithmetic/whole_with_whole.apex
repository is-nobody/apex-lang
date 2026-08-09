// source/tests/arithmetic/whole_with_whole.apex
// Implementation of Whole Number Arithmetic Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: addition of two positive whole numbers
function whole_with_whole_add()
    result = 10 + 3
    assert_equal(result, 13, "10 + 3")

// test: subtraction of whole numbers, expects positive difference
function whole_with_whole_subtract()
    result = 10 - 3
    assert_equal(result, 7, "10 - 3")

// test: integer division of whole numbers (no remainder handling)
function whole_with_whole_divide()
    result = 15 / 3
    assert_equal(result, 5, "15 / 3")

// test: multiplication of two whole numbers
function whole_with_whole_multiply()
    result = 10 * 3
    assert_equal(result, 30, "10 * 3")

// test: modulo operation returns remainder of whole number division
function whole_with_whole_modulo()
    result = 10 % 3
    assert_equal(result, 1, "10 % 3")

// execute all whole number test cases
whole_with_whole_add()
whole_with_whole_subtract()
whole_with_whole_divide()
whole_with_whole_multiply()
whole_with_whole_modulo()

// report completion of whole number arithmetic tests
os.output("[PASS] arithmetic/whole_with_whole.apex")