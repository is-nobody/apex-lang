import os

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function priority_parentheses()
    result = (2 + 3) * 5
    assert_equal(result, 25, "(2 + 3) * 5")
    
    result = 2 * (5 - 3)
    assert_equal(result, 4, "2 * (5 - 3)")
    
    result = 20 / (3 + 2)
    assert_equal(result, 4, "20 / (3 + 2)")
    
    result = (8 - 3) * (2 + 1)
    assert_equal(result, 15, "(8 - 3) * (2 + 1)")

priority_parentheses()

os.output("[PASS] arithmetic/priority_parentheses.apex")