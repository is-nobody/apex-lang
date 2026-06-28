import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function whole_with_whole_add()
    result = 10 + 3
    assert_equal(result, 13, "10 + 3")

function whole_with_whole_subtract()
    result = 10 - 3
    assert_equal(result, 7, "10 - 3")

function whole_with_whole_divide()
    result = 15 / 3
    assert_equal(result, 5, "15 / 3")

function whole_with_whole_multiply()
    result = 10 * 3
    assert_equal(result, 30, "10 * 3")

function whole_with_whole_modulo()
    result = 10 % 3
    assert_equal(result, 1, "10 % 3")

whole_with_whole_add()
whole_with_whole_subtract()
whole_with_whole_divide()
whole_with_whole_multiply()
whole_with_whole_modulo()

if tests_failed == 0
    os.output("[OK] arithmetic/whole_with_whole.apex")