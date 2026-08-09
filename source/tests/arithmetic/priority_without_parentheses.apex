// source/tests/arithmetic/priority_without_parentheses.apex
// Implementation of Operator Priority Tests Without Parentheses for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: verifies standard precedence (* and / before + and -) without parentheses
function priority_mixed_without_parentheses()
    // multiplication before addition
    result = 4 * 3 + 5
    assert_equal(result, 17, "4 * 3 + 5")
    
    // multiplication before addition (reversed order)
    result = 4 + 3 * 5
    assert_equal(result, 19, "4 + 3 * 5")
    
    // division before addition
    result = 30 / 3 + 4
    assert_equal(result, 14, "30 / 3 + 4")
    
    // division before addition (reversed order)
    result = 30 + 9 / 3
    assert_equal(result, 33, "30 + 9 / 3")
    
    // multiplication before subtraction
    result = 18 - 2 * 4
    assert_equal(result, 10, "18 - 2 * 4")
    
    // multiplication before subtraction (reversed order)
    result = 2 * 8 - 3
    assert_equal(result, 13, "2 * 8 - 3")
    
    // division before subtraction
    result = 24 / 4 - 3
    assert_equal(result, 3, "24 / 4 - 3")
    
    // division before subtraction (reversed order)
    result = 24 - 12 / 3
    assert_equal(result, 20, "24 - 12 / 3")
    
    // modulo before addition
    result = 13 % 5 + 2
    assert_equal(result, 5, "13 % 5 + 2")
    
    // modulo before addition (reversed order)
    result = 13 + 5 % 3
    assert_equal(result, 15, "13 + 5 % 3")

// execute all priority test cases
priority_mixed_without_parentheses()

// report completion of operator priority tests
os.output("[PASS] arithmetic/priority_without_parentheses.apex")