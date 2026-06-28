import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function if_single_true()
    result = "small"
    x = 10
    if x > 5
        result = "big"
    assert_equal(result, "big", "x = 10, x > 5 = true")

function if_single_false()
    result = "small"
    x = 3
    if x > 5
        result = "big"
    assert_equal(result, "small", "x = 3, x > 5 = false")

function if_single_negative_true()
    result = "less"
    x = -3
    if x > -5
        result = "greater"
    assert_equal(result, "greater", "x = -3, x > -5 = true")

function if_single_negative_false()
    result = "less"
    x = -10
    if x > -5
        result = "greater"
    assert_equal(result, "less", "x = -10, x > -5 = false")

if_single_true()
if_single_false()
if_single_negative_true()
if_single_negative_false()

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/if_single.apex")