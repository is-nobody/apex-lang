// source/tests/conditionals/comparisons/boolean.apex
// Implementation of Boolean Equality Comparison Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: equality check returns false for different boolean values
function boolean_equal_false()
    result = true == false
    assert_equal(result, false, "true == false")

// test: equality check returns true for identical boolean values
function boolean_equal_true()
    result = true == true
    assert_equal(result, true, "true == true")

// test: inequality check returns false when values are equal
function boolean_not_equal_false()
    result = true != true
    assert_equal(result, false, "true != true")

// test: inequality check returns true when values differ
function boolean_not_equal_true()
    result = true != false
    assert_equal(result, true, "true != false")

// execute all boolean comparison test cases
boolean_equal_false()
boolean_equal_true()
boolean_not_equal_false()
boolean_not_equal_true()

// report completion of boolean comparison tests
os.output("[PASS] conditionals/comparisons/boolean.apex")