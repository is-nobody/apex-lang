// source/tests/conditionals/comparisons/whole.apex
// Implementation of Whole Number Comparison Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// == operator: equality comparison for whole numbers
function whole_equal_false()
    result = 5 == 3
    assert_equal(result, false, "5 == 3")

function whole_equal_true()
    result = 5 == 5
    assert_equal(result, true, "5 == 5")

function whole_equal_negative_false()
    result = -5 == -3
    assert_equal(result, false, "-5 == -3")

function whole_equal_negative_true()
    result = -5 == -5
    assert_equal(result, true, "-5 == -5")

// != operator: inequality comparison for whole numbers
function whole_not_equal_false()
    result = 5 != 5
    assert_equal(result, false, "5 != 5")

function whole_not_equal_true()
    result = 5 != 3
    assert_equal(result, true, "5 != 3")

function whole_not_equal_negative_false()
    result = -5 != -5
    assert_equal(result, false, "-5 != -5")

function whole_not_equal_negative_true()
    result = -5 != -3
    assert_equal(result, true, "-5 != -3")

// < operator: less than comparison for whole numbers
function whole_less_than_equal_false()
    result = 5 < 5
    assert_equal(result, false, "5 < 5")

function whole_less_than_false()
    result = 10 < 5
    assert_equal(result, false, "10 < 5")

function whole_less_than_true()
    result = 5 < 10
    assert_equal(result, true, "5 < 10")

function whole_less_than_negative_equal_false()
    result = -5 < -5
    assert_equal(result, false, "-5 < -5")

function whole_less_than_negative_false()
    result = -5 < -10
    assert_equal(result, false, "-5 < -10")

function whole_less_than_negative_true()
    result = -10 < -5
    assert_equal(result, true, "-10 < -5")

// > operator: greater than comparison for whole numbers
function whole_greater_than_equal_false()
    result = 5 > 5
    assert_equal(result, false, "5 > 5")

function whole_greater_than_false()
    result = 5 > 10
    assert_equal(result, false, "5 > 10")

function whole_greater_than_true()
    result = 10 > 5
    assert_equal(result, true, "10 > 5")

function whole_greater_than_negative_equal_false()
    result = -5 > -5
    assert_equal(result, false, "-5 > -5")

function whole_greater_than_negative_false()
    result = -10 > -5
    assert_equal(result, false, "-10 > -5")

function whole_greater_than_negative_true()
    result = -5 > -10
    assert_equal(result, true, "-5 > -10")

// <= operator: less than or equal comparison for whole numbers
function whole_less_equal_equal_true()
    result = 5 <= 5
    assert_equal(result, true, "5 <= 5")

function whole_less_equal_false()
    result = 10 <= 5
    assert_equal(result, false, "10 <= 5")

function whole_less_equal_true()
    result = 5 <= 10
    assert_equal(result, true, "5 <= 10")

function whole_less_equal_negative_equal_true()
    result = -5 <= -5
    assert_equal(result, true, "-5 <= -5")

function whole_less_equal_negative_false()
    result = -5 <= -10
    assert_equal(result, false, "-5 <= -10")

function whole_less_equal_negative_true()
    result = -10 <= -5
    assert_equal(result, true, "-10 <= -5")

// >= operator: greater than or equal comparison for whole numbers
function whole_greater_equal_equal_true()
    result = 5 >= 5
    assert_equal(result, true, "5 >= 5")

function whole_greater_equal_false()
    result = 5 >= 10
    assert_equal(result, false, "5 >= 10")

function whole_greater_equal_true()
    result = 10 >= 5
    assert_equal(result, true, "10 >= 5")

function whole_greater_equal_negative_equal_true()
    result = -5 >= -5
    assert_equal(result, true, "-5 >= -5")

function whole_greater_equal_negative_false()
    result = -10 >= -5
    assert_equal(result, false, "-10 >= -5")

function whole_greater_equal_negative_true()
    result = -5 >= -10
    assert_equal(result, true, "-5 >= -10")

// execute all whole number comparison test cases
whole_equal_false()
whole_equal_true()
whole_equal_negative_false()
whole_equal_negative_true()
whole_not_equal_false()
whole_not_equal_true()
whole_not_equal_negative_false()
whole_not_equal_negative_true()
whole_less_than_equal_false()
whole_less_than_false()
whole_less_than_true()
whole_less_than_negative_equal_false()
whole_less_than_negative_false()
whole_less_than_negative_true()
whole_greater_than_equal_false()
whole_greater_than_false()
whole_greater_than_true()
whole_greater_than_negative_equal_false()
whole_greater_than_negative_false()
whole_greater_than_negative_true()
whole_less_equal_equal_true()
whole_less_equal_false()
whole_less_equal_true()
whole_less_equal_negative_equal_true()
whole_less_equal_negative_false()
whole_less_equal_negative_true()
whole_greater_equal_equal_true()
whole_greater_equal_false()
whole_greater_equal_true()
whole_greater_equal_negative_equal_true()
whole_greater_equal_negative_false()
whole_greater_equal_negative_true()

// report completion of whole number comparison tests
os.output("[PASS] conditionals/comparisons/whole.apex")