// string_conversion.apex
import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function string_from_int()
    result = string(42)
    assert_equal(result, "42", "string(42)")

function string_from_decimal()
    result = string(3.14)
    assert_equal(result, "3.14", "string(3.14)")

function string_from_true()
    result = string(true)
    assert_equal(result, "true", "string(true)")

function string_from_false()
    result = string(false)
    assert_equal(result, "false", "string(false)")

function string_from_negative()
    result = string(-10)
    assert_equal(result, "-10", "string(-10)")

function string_from_zero()
    result = string(0)
    assert_equal(result, "0", "string(0)")

function string_from_none()
    result = string(none)
    assert_equal(result, "none", "string(none)")

string_from_int()
string_from_decimal()
string_from_true()
string_from_false()
string_from_negative()
string_from_zero()
string_from_none()

if tests_failed == 0
    os.output("[OK] type_conversion/string.apex")