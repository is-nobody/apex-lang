import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function decimal_with_decimal_add()
    result = 3.5 + 1.5
    assert_equal(result, 5, "3.5 + 1.5")

function decimal_with_decimal_subtract()
    result = 3.5 - 1.5
    assert_equal(result, 2, "3.5 - 1.5")

function decimal_with_decimal_divide()
    result = 3.5 / 2.8
    assert_equal(result, 1.25, "3.5 / 2.8")

function decimal_with_decimal_multiply()
    result = 3.5 * 1.5
    assert_equal(result, 5.25, "3.5 * 1.5")

function decimal_with_decimal_modulo()
    result = 3.5 % 1.5
    assert_equal(result, 0.5, "3.5 % 1.5")

decimal_with_decimal_add()
decimal_with_decimal_subtract()
decimal_with_decimal_divide()
decimal_with_decimal_multiply()
decimal_with_decimal_modulo()

if tests_failed == 0
    os.output("[OK] arithmetic/decimal_with_decimal.apex")