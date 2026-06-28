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
function whole_equal_false()
    result = 5 == 3
    assert_equal(result, false, "5 == 3")

function whole_equal_true()
    result = 5 == 5
    assert_equal(result, true, "5 == 5")

function whole_equal_negative_false()
    result = -5 == -3
    assert_equal(result, false, "-5 == -3")

function whole_equal_negative_true()
    result = -5 == -5
    assert_equal(result, true, "-5 == -5")

// !=
function whole_not_equal_false()
    result = 5 != 5
    assert_equal(result, false, "5 != 5")

function whole_not_equal_true()
    result = 5 != 3
    assert_equal(result, true, "5 != 3")

function whole_not_equal_negative_false()
    result = -5 != -5
    assert_equal(result, false, "-5 != -5")

function whole_not_equal_negative_true()
    result = -5 != -3
    assert_equal(result, true, "-5 != -3")

// <
function whole_less_than_equal_false()
    result = 5 < 5
    assert_equal(result, false, "5 < 5")

function whole_less_than_false()
    result = 10 < 5
    assert_equal(result, false, "10 < 5")

function whole_less_than_true()
    result = 5 < 10
    assert_equal(result, true, "5 < 10")

function whole_less_than_negative_equal_false()
    result = -5 < -5
    assert_equal(result, false, "-5 < -5")

function whole_less_than_negative_false()
    result = -5 < -10
    assert_equal(result, false, "-5 < -10")

function whole_less_than_negative_true()
    result = -10 < -5
    assert_equal(result, true, "-10 < -5")

// >
function whole_greater_than_equal_false()
    result = 5 > 5
    assert_equal(result, false, "5 > 5")

function whole_greater_than_false()
    result = 5 > 10
    assert_equal(result, false, "5 > 10")

function whole_greater_than_true()
    result = 10 > 5
    assert_equal(result, true, "10 > 5")

function whole_greater_than_negative_equal_false()
    result = -5 > -5
    assert_equal(result, false, "-5 > -5")

function whole_greater_than_negative_false()
    result = -10 > -5
    assert_equal(result, false, "-10 > -5")

function whole_greater_than_negative_true()
    result = -5 > -10
    assert_equal(result, true, "-5 > -10")

// <=
function whole_less_equal_equal_true()
    result = 5 <= 5
    assert_equal(result, true, "5 <= 5")

function whole_less_equal_false()
    result = 10 <= 5
    assert_equal(result, false, "10 <= 5")

function whole_less_equal_true()
    result = 5 <= 10
    assert_equal(result, true, "5 <= 10")

function whole_less_equal_negative_equal_true()
    result = -5 <= -5
    assert_equal(result, true, "-5 <= -5")

function whole_less_equal_negative_false()
    result = -5 <= -10
    assert_equal(result, false, "-5 <= -10")

function whole_less_equal_negative_true()
    result = -10 <= -5
    assert_equal(result, true, "-10 <= -5")

// >=
function whole_greater_equal_equal_true()
    result = 5 >= 5
    assert_equal(result, true, "5 >= 5")

function whole_greater_equal_false()
    result = 5 >= 10
    assert_equal(result, false, "5 >= 10")

function whole_greater_equal_true()
    result = 10 >= 5
    assert_equal(result, true, "10 >= 5")

function whole_greater_equal_negative_equal_true()
    result = -5 >= -5
    assert_equal(result, true, "-5 >= -5")

function whole_greater_equal_negative_false()
    result = -10 >= -5
    assert_equal(result, false, "-10 >= -5")

function whole_greater_equal_negative_true()
    result = -5 >= -10
    assert_equal(result, true, "-5 >= -10")

whole_equal_false()
whole_equal_true()
whole_equal_negative_false()
whole_equal_negative_true()
whole_not_equal_false()
whole_not_equal_true()
whole_not_equal_negative_false()
whole_not_equal_negative_true()
whole_less_than_equal_false()
whole_less_than_false()
whole_less_than_true()
whole_less_than_negative_equal_false()
whole_less_than_negative_false()
whole_less_than_negative_true()
whole_greater_than_equal_false()
whole_greater_than_false()
whole_greater_than_true()
whole_greater_than_negative_equal_false()
whole_greater_than_negative_false()
whole_greater_than_negative_true()
whole_less_equal_equal_true()
whole_less_equal_false()
whole_less_equal_true()
whole_less_equal_negative_equal_true()
whole_less_equal_negative_false()
whole_less_equal_negative_true()
whole_greater_equal_equal_true()
whole_greater_equal_false()
whole_greater_equal_true()
whole_greater_equal_negative_equal_true()
whole_greater_equal_negative_false()
whole_greater_equal_negative_true()

if tests_failed == 0
    os.output("[OK] conditionals/comparisons/whole.apex")