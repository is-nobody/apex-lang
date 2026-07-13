import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function none_equal_none()
    result = none == none
    assert_equal(result, true, "none == none")

function none_not_equal_none()
    result = none != none
    assert_equal(result, false, "none != none")

function none_equal_number()
    result = none == 42
    assert_equal(result, false, "none == 42")

function none_not_equal_number()
    result = none != 42
    assert_equal(result, true, "none != 42")

function none_equal_string()
    result = none == "hello"
    assert_equal(result, false, "none == 'hello'")

function none_not_equal_string()
    result = none != "hello"
    assert_equal(result, true, "none != 'hello'")

function none_equal_boolean()
    result = none == true
    assert_equal(result, false, "none == true")
    
    result = none == false
    assert_equal(result, false, "none == false")

function none_not_equal_boolean()
    result = none != true
    assert_equal(result, true, "none != true")
    
    result = none != false
    assert_equal(result, true, "none != false")

function variable_none()
    x = none
    assert_equal(x, none, "x = none")
    assert_equal(x == none, true, "x == none")
    assert_equal(x != none, false, "x != none")

function return_none()
    return none

function return_none()
    result = return_none()
    assert_equal(result, none, "function returns none")

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

if tests_failed == 0
    os.output("[OK] conditionals/comparisons/none.apex")