// source/tests/conditionals/comparisons/string.apex
// Implementation of String Equality Comparison Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// == operator: string equality returns false for different values
function string_equal_false()
    result = "hello" == "world"
    assert_equal(result, false, "hello == world")

// == operator: string equality returns true for identical values
function string_equal_true()
    result = "hello" == "hello"
    assert_equal(result, true, "hello == hello")

// != operator: string inequality returns false for identical values
function string_not_equal_false()
    result = "hello" != "hello"
    assert_equal(result, false, "hello != hello")

// != operator: string inequality returns true for different values
function string_not_equal_true()
    result = "hello" != "world"
    assert_equal(result, true, "hello != world")

// execute all string comparison test cases
string_equal_false()
string_equal_true()
string_not_equal_false()
string_not_equal_true()

// report completion of string comparison tests
os.output("[PASS] conditionals/comparisons/string.apex")