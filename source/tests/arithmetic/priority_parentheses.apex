// source/tests/arithmetic/priority_parentheses.apex
// Implementation of Operator Priority Tests With Parentheses for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: verifies parentheses override standard operator precedence
function priority_parentheses()
    // parentheses force addition before multiplication
    result = (2 + 3) * 5
    assert_equal(result, 25, "(2 + 3) * 5")
    
    // parentheses force subtraction before multiplication
    result = 2 * (5 - 3)
    assert_equal(result, 4, "2 * (5 - 3)")
    
    // parentheses force addition before division
    result = 20 / (3 + 2)
    assert_equal(result, 4, "20 / (3 + 2)")
    
    // nested parentheses evaluate inner groups first
    result = (8 - 3) * (2 + 1)
    assert_equal(result, 15, "(8 - 3) * (2 + 1)")

// execute all parentheses priority test cases
priority_parentheses()

// report completion of parentheses operator tests
os.output("[PASS] arithmetic/priority_parentheses.apex")