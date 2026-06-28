import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function boolean_equal_false()
    result = true == false
    assert_equal(result, false, "true == false")

function boolean_equal_true()
    result = true == true
    assert_equal(result, true, "true == true")

function boolean_not_equal_false()
    result = true != true
    assert_equal(result, false, "true != true")

function boolean_not_equal_true()
    result = true != false
    assert_equal(result, true, "true != false")

boolean_equal_false()
boolean_equal_true()
boolean_not_equal_false()
boolean_not_equal_true()

if tests_failed == 0
    os.output("[OK] conditionals/comparisons/boolean.apex")