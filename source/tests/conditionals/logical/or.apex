// source/tests/conditionals/logical/or.apex
// Implementation of Logical OR Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: OR returns false only when both operands are false
function or_false_false()
    result = false or false
    assert_equal(result, false, "false or false")

// test: OR returns true when first operand is false, second is true
function or_false_true()
    result = false or true
    assert_equal(result, true, "false or true")

// test: OR returns true when first operand is true, second is false
function or_true_false()
    result = true or false
    assert_equal(result, true, "true or false")

// test: OR returns true when both operands are true
function or_true_true()
    result = true or true
    assert_equal(result, true, "true or true")

// execute all logical OR test cases
or_false_false()
or_false_true()
or_true_false()
or_true_true()

// report completion of logical OR tests
os.output("[PASS] conditionals/logical/or.apex")