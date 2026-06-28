import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function not_false()
    result = not false
    assert_equal(result, true, "not false")

function not_true()
    result = not true
    assert_equal(result, false, "not true")

not_false()
not_true()

if tests_failed == 0
    os.output("[OK] conditionals/logical/not.apex")