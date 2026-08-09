import os

function assert_equal(actual, expected, name)
    if actual != expected
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

os.output("[PASS] conditionals/if-elif-else/if_single.apex")