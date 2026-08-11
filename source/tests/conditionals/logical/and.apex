// source/tests/conditionals/logical/and.apex
// Implementation of Logical AND Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: AND returns false when both operands are false
function and_false_false()
    result = (false == true) and (false == true)
    assert_equal(result, false, "false and false")

// test: AND returns false when first operand is false, second is true
function and_false_true()
    result = (false == true) and (true == true)
    assert_equal(result, false, "false and true")

// test: AND returns false when first operand is true, second is false
function and_true_false()
    result = (true == true) and (false == true)
    assert_equal(result, false, "true and false")

// test: AND returns true only when both operands are true
function and_true_true()
    result = (true == true) and (true == true)
    assert_equal(result, true, "true and true")

// execute all logical AND test cases
and_false_false()
and_false_true()
and_true_false()
and_true_true()

// report completion of logical AND tests
os.output("[PASS] conditionals/logical/and.apex")