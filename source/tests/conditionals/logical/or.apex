import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function or_false_false()
    result = false or false
    assert_equal(result, false, "false or false")

function or_false_true()
    result = false or true
    assert_equal(result, true, "false or true")

function or_true_false()
    result = true or false
    assert_equal(result, true, "true or false")

function or_true_true()
    result = true or true
    assert_equal(result, true, "true or true")

or_false_false()
or_false_true()
or_true_false()
or_true_true()

os.output("[PASS] conditionals/logical/or.apex")