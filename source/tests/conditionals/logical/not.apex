import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function not_false()
    result = not false
    assert_equal(result, true, "not false")

function not_true()
    result = not true
    assert_equal(result, false, "not true")

not_false()
not_true()

os.output("[PASS] conditionals/logical/not.apex")