// source/tests/type_conversion/number.apex
// Implementation of Number Type Conversion Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: converts integer string to number
function number_from_int_string()
    result = number("42")
    assert_equal(result, 42, "number('42')")

// test: converts decimal string to number
function number_from_decimal_string()
    result = number("3.14")
    assert_equal(result, 3.14, "number('3.14')")

// test: boolean true returns none (not numeric)
function number_from_true()
    result = number(true)
    assert_equal(result, none, "number(true)")

// test: boolean false returns none (not numeric)
function number_from_false()
    result = number(false)
    assert_equal(result, none, "number(false)")

// test: non-numeric string returns none
function number_from_invalid_string()
    result = number("hello")
    assert_equal(result, none, "number('hello')")

// test: converts negative number string
function number_from_negative_string()
    result = number("-5")
    assert_equal(result, -5, "number('-5')")

// test: converts zero string
function number_from_zero_string()
    result = number("0")
    assert_equal(result, 0, "number('0')")

// test: none input returns none
function number_from_none()
    result = number(none)
    assert_equal(result, none, "number(none)")

// test: table input returns none
function number_from_table()
    result = number(["number"])
    assert_equal(result, none, "number(['cherry'])")

// execute all number conversion test cases
number_from_int_string()
number_from_decimal_string()
number_from_true()
number_from_false()
number_from_invalid_string()
number_from_negative_string()
number_from_zero_string()
number_from_none()
number_from_table()

// report completion of number conversion tests
os.output("[PASS] type_conversion/number.apex")