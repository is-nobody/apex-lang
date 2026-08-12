// source/tests/type_conversion/string.apex
// Implementation of String Type Conversion Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: converts integer to string
function string_from_int()
    result = string(42)
    assert_equal(result, "42", "string(42)")

// test: converts decimal to string
function string_from_decimal()
    result = string(3.14)
    assert_equal(result, "3.14", "string(3.14)")

// test: converts string to string (returns same value)
function string_from_string()
    result = string("hello world")
    assert_equal(result, "hello world", "string('hello world')")

// test: converts empty string to string
function string_from_empty_string()
    result = string("")
    assert_equal(result, "", "string('')")

// test: converts boolean true to string
function string_from_true()
    result = string(true)
    assert_equal(result, "true", "string(true)")

// test: converts boolean false to string
function string_from_false()
    result = string(false)
    assert_equal(result, "false", "string(false)")

// test: converts negative number to string
function string_from_negative()
    result = string(-10)
    assert_equal(result, "-10", "string(-10)")

// test: converts zero to string
function string_from_zero()
    result = string(0)
    assert_equal(result, "0", "string(0)")

// test: converts none to string representation
function string_from_none()
    result = string(none)
    assert_equal(result, "none", "string(none)")

// test: converts table to pretty-printed string
function string_from_table()
    result = string(["cherry"])
    assert_equal(result, "[\n    1 = \"cherry\"\n]", "string(['cherry'])")

// execute all string conversion test cases
string_from_int()
string_from_decimal()
string_from_string()
string_from_empty_string()
string_from_true()
string_from_false()
string_from_negative()
string_from_zero()
string_from_none()
string_from_table()

// report completion of string conversion tests
os.output("[PASS] type_conversion/string.apex")