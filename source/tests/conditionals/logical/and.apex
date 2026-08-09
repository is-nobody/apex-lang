import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function and_false_false()
    result = false and false
    assert_equal(result, false, "false and false")

function and_false_true()
    result = false and true
    assert_equal(result, false, "false and true")

function and_true_false()
    result = true and false
    assert_equal(result, false, "true and false")

function and_true_true()
    result = true and true
    assert_equal(result, true, "true and true")

and_false_false()
and_false_true()
and_true_false()
and_true_true()

os.output("[PASS] conditionals/logical/and.apex")