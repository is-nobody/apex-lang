// source/tests/conditionals/logical/not.apex
// Implementation of Logical NOT Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: not operator inverts false to true
function not_false()
    result = not false
    assert_equal(result, true, "not false")

// test: not operator inverts true to false
function not_true()
    result = not true
    assert_equal(result, false, "not true")

// test: not with comparison - false comparison becomes true
function not_comparison_false()
    result = not (5 > 10)
    assert_equal(result, true, "not with false comparison")

// test: not with comparison - true comparison becomes false
function not_comparison_true()
    result = not (10 > 5)
    assert_equal(result, false, "not with true comparison")

// test: not with equality comparison
function not_equality()
    result = not (5 == 5)
    assert_equal(result, false, "not with equality true")

// test: not with inequality comparison
function not_inequality()
    result = not (5 != 5)
    assert_equal(result, true, "not with inequality false")

// test: not with arithmetic comparison
function not_arithmetic()
    x = 10
    y = 20
    result = not ((x + y) > 100)
    assert_equal(result, true, "not with arithmetic comparison")

// test: not with string comparison
function not_string_comparison()
    name = "Alice"
    result = not (name == "Bob")
    assert_equal(result, true, "not with string comparison")

// test: not with type check
function not_type_check()
    value = 42
    result = not (type(value) == "string")
    assert_equal(result, true, "not with type check")

// test: not with none comparison
function not_none_comparison()
    value = "something"
    result = not (value == none)
    assert_equal(result, true, "not with none comparison")

// test: not with none value
function not_none_value()
    value = none
    result = not (value != none)
    assert_equal(result, true, "not with none value false")

// test: not with multiple comparisons using and
function not_with_and()
    result = not ((5 > 3) and (10 > 5))
    assert_equal(result, false, "not with and expression")

// test: not with multiple comparisons using or
function not_with_or()
    result = not ((5 > 10) or (3 > 5))
    assert_equal(result, true, "not with or expression")

// test: not with nested not - double negation
function not_double_negation()
    result = not (not (5 > 3))
    assert_equal(result, true, "double negation")

// test: not with nested not - triple negation
function not_triple_negation()
    result = not (not (not (5 > 3)))
    assert_equal(result, false, "triple negation")

// test: not with complex expression
function not_complex_expression()
    x = 10
    y = 5
    z = 3
    result = not ((x > y) and ((y > z) or (z > y)))
    assert_equal(result, false, "not with complex expression")

// test: not with boolean variable
function not_boolean_variable()
    is_active = true
    result = not (is_active == true)
    assert_equal(result, false, "not with boolean variable")

// test: not with comparison result stored in variable
function not_stored_comparison()
    comparison = 10 > 5
    result = not comparison
    assert_equal(result, false, "not with stored comparison")

// execute all logical not test cases
not_false()
not_true()
not_comparison_false()
not_comparison_true()
not_equality()
not_inequality()
not_arithmetic()
not_string_comparison()
not_type_check()
not_none_comparison()
not_none_value()
not_with_and()
not_with_or()
not_double_negation()
not_triple_negation()
not_complex_expression()
not_boolean_variable()
not_stored_comparison()

// report completion of logical not tests
os.output("[PASS] conditionals/logical/not.apex")