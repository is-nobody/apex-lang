import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// ==
function decimal_equal_false()
    result = 3.5 == 2.5
    assert_equal(result, false, "3.5 == 2.5")

function decimal_equal_true()
    result = 3.5 == 3.5
    assert_equal(result, true, "3.5 == 3.5")

function decimal_equal_negative_false()
    result = -5.5 == -3.5
    assert_equal(result, false, "-5.5 == -3.5")

function decimal_equal_negative_true()
    result = -5.5 == -5.5
    assert_equal(result, true, "-5.5 == -5.5")

// !=
function decimal_not_equal_false()
    result = 3.5 != 3.5
    assert_equal(result, false, "3.5 != 3.5")

function decimal_not_equal_true()
    result = 3.5 != 2.5
    assert_equal(result, true, "3.5 != 2.5")

function decimal_not_equal_negative_false()
    result = -5.5 != -5.5
    assert_equal(result, false, "-5.5 != -5.5")

function decimal_not_equal_negative_true()
    result = -5.5 != -3.5
    assert_equal(result, true, "-5.5 != -3.5")

// <
function decimal_less_than_equal_false()
    result = 3.5 < 3.5
    assert_equal(result, false, "3.5 < 3.5")

function decimal_less_than_false()
    result = 5.5 < 3.5
    assert_equal(result, false, "5.5 < 3.5")

function decimal_less_than_true()
    result = 3.5 < 5.5
    assert_equal(result, true, "3.5 < 5.5")

function decimal_less_than_negative_equal_false()
    result = -5.5 < -5.5
    assert_equal(result, false, "-5.5 < -5.5")

function decimal_less_than_negative_false()
    result = -5.5 < -10.5
    assert_equal(result, false, "-5.5 < -10.5")

function decimal_less_than_negative_true()
    result = -10.5 < -5.5
    assert_equal(result, true, "-10.5 < -5.5")

// >
function decimal_greater_than_equal_false()
    result = 5.5 > 5.5
    assert_equal(result, false, "5.5 > 5.5")

function decimal_greater_than_false()
    result = 3.5 > 5.5
    assert_equal(result, false, "3.5 > 5.5")

function decimal_greater_than_true()
    result = 5.5 > 3.5
    assert_equal(result, true, "5.5 > 3.5")

function decimal_greater_than_negative_equal_false()
    result = -5.5 > -5.5
    assert_equal(result, false, "-5.5 > -5.5")

function decimal_greater_than_negative_false()
    result = -10.5 > -5.5
    assert_equal(result, false, "-10.5 > -5.5")

function decimal_greater_than_negative_true()
    result = -5.5 > -10.5
    assert_equal(result, true, "-5.5 > -10.5")

// <=
function decimal_less_equal_equal_true()
    result = 3.5 <= 3.5
    assert_equal(result, true, "3.5 <= 3.5")

function decimal_less_equal_false()
    result = 5.5 <= 3.5
    assert_equal(result, false, "5.5 <= 3.5")

function decimal_less_equal_true()
    result = 3.5 <= 5.5
    assert_equal(result, true, "3.5 <= 5.5")

function decimal_less_equal_negative_equal_true()
    result = -5.5 <= -5.5
    assert_equal(result, true, "-5.5 <= -5.5")

function decimal_less_equal_negative_false()
    result = -5.5 <= -10.5
    assert_equal(result, false, "-5.5 <= -10.5")

function decimal_less_equal_negative_true()
    result = -10.5 <= -5.5
    assert_equal(result, true, "-10.5 <= -5.5")

// >=
function decimal_greater_equal_equal_true()
    result = 5.5 >= 5.5
    assert_equal(result, true, "5.5 >= 5.5")

function decimal_greater_equal_false()
    result = 3.5 >= 5.5
    assert_equal(result, false, "3.5 >= 5.5")

function decimal_greater_equal_true()
    result = 5.5 >= 3.5
    assert_equal(result, true, "5.5 >= 3.5")

function decimal_greater_equal_negative_equal_true()
    result = -5.5 >= -5.5
    assert_equal(result, true, "-5.5 >= -5.5")

function decimal_greater_equal_negative_false()
    result = -10.5 >= -5.5
    assert_equal(result, false, "-10.5 >= -5.5")

function decimal_greater_equal_negative_true()
    result = -5.5 >= -10.5
    assert_equal(result, true, "-5.5 >= -10.5")

decimal_equal_false()
decimal_equal_true()
decimal_equal_negative_false()
decimal_equal_negative_true()
decimal_not_equal_false()
decimal_not_equal_true()
decimal_not_equal_negative_false()
decimal_not_equal_negative_true()
decimal_less_than_equal_false()
decimal_less_than_false()
decimal_less_than_true()
decimal_less_than_negative_equal_false()
decimal_less_than_negative_false()
decimal_less_than_negative_true()
decimal_greater_than_equal_false()
decimal_greater_than_false()
decimal_greater_than_true()
decimal_greater_than_negative_equal_false()
decimal_greater_than_negative_false()
decimal_greater_than_negative_true()
decimal_less_equal_equal_true()
decimal_less_equal_false()
decimal_less_equal_true()
decimal_less_equal_negative_equal_true()
decimal_less_equal_negative_false()
decimal_less_equal_negative_true()
decimal_greater_equal_equal_true()
decimal_greater_equal_false()
decimal_greater_equal_true()
decimal_greater_equal_negative_equal_true()
decimal_greater_equal_negative_false()
decimal_greater_equal_negative_true()

if tests_failed == 0
    os.output("[OK] conditionals/comparisons/decimal.apex")