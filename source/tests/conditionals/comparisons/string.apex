import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function string_equal_false()
    result = "hello" == "world"
    assert_equal(result, false, "hello == world")

function string_equal_true()
    result = "hello" == "hello"
    assert_equal(result, true, "hello == hello")

function string_not_equal_false()
    result = "hello" != "hello"
    assert_equal(result, false, "hello != hello")

function string_not_equal_true()
    result = "hello" != "world"
    assert_equal(result, true, "hello != world")

string_equal_false()
string_equal_true()
string_not_equal_false()
string_not_equal_true()

os.output("[PASS] conditionals/comparisons/string.apex")