import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function number_from_int_string()
    result = number("42")
    assert_equal(result, 42, "number('42')")

function number_from_decimal_string()
    result = number("3.14")
    assert_equal(result, 3.14, "number('3.14')")

function number_from_true()
    result = number(true)
    assert_equal(result, none, "number(true)")

function number_from_false()
    result = number(false)
    assert_equal(result, none, "number(false)")

function number_from_invalid_string()
    result = number("hello")
    assert_equal(result, none, "number('hello')")

function number_from_negative_string()
    result = number("-5")
    assert_equal(result, -5, "number('-5')")

function number_from_zero_string()
    result = number("0")
    assert_equal(result, 0, "number('0')")

function number_from_none()
    result = number(none)
    assert_equal(result, none, "number(none)")

function number_from_table()
    result = number(["number"])
    assert_equal(result, none, "number(['cherry'])")

number_from_int_string()
number_from_decimal_string()
number_from_true()
number_from_false()
number_from_invalid_string()
number_from_negative_string()
number_from_zero_string()
number_from_none()
number_from_table()

if tests_failed == 0
    os.output("[OK] type_conversion/number.apex")