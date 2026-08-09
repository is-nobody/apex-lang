// source/tests/conditionals/if-elif-else/if_single.apex
// Implementation of Single If Conditional Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: if block executes when condition is true
function if_single_true()
    result = "small"
    x = 10
    if x > 5
        result = "big"
    assert_equal(result, "big", "x = 10, x > 5 = true")

// test: if block skipped when condition is false, variable unchanged
function if_single_false()
    result = "small"
    x = 3
    if x > 5
        result = "big"
    assert_equal(result, "small", "x = 3, x > 5 = false")

// test: negative numbers, if block executes when condition holds
function if_single_negative_true()
    result = "less"
    x = -3
    if x > -5
        result = "greater"
    assert_equal(result, "greater", "x = -3, x > -5 = true")

// test: negative numbers, if block skipped when condition fails
function if_single_negative_false()
    result = "less"
    x = -10
    if x > -5
        result = "greater"
    assert_equal(result, "less", "x = -10, x > -5 = false")

// execute all single if test cases
if_single_true()
if_single_false()
if_single_negative_true()
if_single_negative_false()

// report completion of single if tests
os.output("[PASS] conditionals/if-elif-else/if_single.apex")