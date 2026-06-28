import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function priority_mixed_without_parentheses()
    result = 4 * 3 + 5
    assert_equal(result, 17, "4 * 3 + 5")
    
    result = 4 + 3 * 5
    assert_equal(result, 19, "4 + 3 * 5")
    
    result = 30 / 3 + 4
    assert_equal(result, 14, "30 / 3 + 4")
    
    result = 30 + 9 / 3
    assert_equal(result, 33, "30 + 9 / 3")
    
    result = 18 - 2 * 4
    assert_equal(result, 10, "18 - 2 * 4")
    
    result = 2 * 8 - 3
    assert_equal(result, 13, "2 * 8 - 3")
    
    result = 24 / 4 - 3
    assert_equal(result, 3, "24 / 4 - 3")
    
    result = 24 - 12 / 3
    assert_equal(result, 20, "24 - 12 / 3")
    
    result = 13 % 5 + 2
    assert_equal(result, 5, "13 % 5 + 2")
    
    result = 13 + 5 % 3
    assert_equal(result, 15, "13 + 5 % 3")

priority_mixed_without_parentheses()

if tests_failed == 0
    os.output("[OK] arithmetic/priority_without_parentheses.apex")