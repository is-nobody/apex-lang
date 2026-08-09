import os

function assert_equal(actual, expected, name)
    if actual != expected
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

os.output("[PASS] arithmetic/decimal_with_decimal.apex")