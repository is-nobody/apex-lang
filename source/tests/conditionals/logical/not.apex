// source/tests/conditionals/logical/not.apex
// Implementation of Logical NOT Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: not operator inverts false to true
function not_false()
    result = not false
    assert_equal(result, true, "not false")

// test: not operator inverts true to false
function not_true()
    result = not true
    assert_equal(result, false, "not true")

// execute all logical not test cases
not_false()
not_true()

// report completion of logical not tests
os.output("[PASS] conditionals/logical/not.apex")