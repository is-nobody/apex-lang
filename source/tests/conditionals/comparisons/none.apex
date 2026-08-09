// source/tests/conditionals/comparisons/none.apex
// Implementation of None Value Comparison Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// == operator: none equality with itself yields true
function none_equal_none()
    result = none == none
    assert_equal(result, true, "none == none")

// != operator: none inequality with itself yields false
function none_not_equal_none()
    result = none != none
    assert_equal(result, false, "none != none")

// == operator: none compared to number returns false
function none_equal_number()
    result = none == 42
    assert_equal(result, false, "none == 42")

// != operator: none compared to number returns true
function none_not_equal_number()
    result = none != 42
    assert_equal(result, true, "none != 42")

// == operator: none compared to string returns false
function none_equal_string()
    result = none == "hello"
    assert_equal(result, false, "none == 'hello'")

// != operator: none compared to string returns true
function none_not_equal_string()
    result = none != "hello"
    assert_equal(result, true, "none != 'hello'")

// == operator: none compared to boolean values returns false
function none_equal_boolean()
    result = none == true
    assert_equal(result, false, "none == true")
    
    result = none == false
    assert_equal(result, false, "none == false")

// != operator: none compared to boolean values returns true
function none_not_equal_boolean()
    result = none != true
    assert_equal(result, true, "none != true")
    
    result = none != false
    assert_equal(result, true, "none != false")

// test: variable assignment of none and subsequent comparisons
function variable_none()
    x = none
    assert_equal(x, none, "x = none")
    assert_equal(x == none, true, "x == none")
    assert_equal(x != none, false, "x != none")

// helper: function that explicitly returns none
function return_none()
    return none

// test: function returning none is correctly identified
function return_none()
    result = return_none()
    assert_equal(result, none, "function returns none")

// execute all none comparison test cases
none_equal_none()
none_not_equal_none()
none_equal_number()
none_not_equal_number()
none_equal_string()
none_not_equal_string()
none_equal_boolean()
none_not_equal_boolean()
variable_none()
return_none()

// report completion of none comparison tests
os.output("[PASS] conditionals/comparisons/none.apex")