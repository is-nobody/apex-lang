import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function whole_with_decimal_add()
    result = 5 + 2.5
    assert_equal(result, 7.5, "5 + 2.5")

function whole_with_decimal_subtract()
    result = 5 - 2.5
    assert_equal(result, 2.5, "5 - 2.5")

function whole_with_decimal_divide()
    result = 5 / 2.5
    assert_equal(result, 2.0, "5 / 2.5")

function whole_with_decimal_multiply()
    result = 5 * 2.5
    assert_equal(result, 12.5, "5 * 2.5")

function whole_with_decimal_modulo()
    result = 5 % 2.5
    assert_equal(result, 0, "5 % 2.5")

whole_with_decimal_add()
whole_with_decimal_subtract()
whole_with_decimal_divide()
whole_with_decimal_multiply()
whole_with_decimal_modulo()

os.output("[PASS] arithmetic/whole_with_decimal.apex")