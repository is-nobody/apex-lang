import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
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

if tests_failed == 0
    os.output("[OK] arithmetic/priority_parentheses.apex")