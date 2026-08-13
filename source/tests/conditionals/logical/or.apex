// source/tests/conditionals/logical/or.apex
// Implementation of Logical OR Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: OR returns false only when both operands are false
function or_false_false()
    result = (false == true) or (false == true)
    assert_equal(result, false, "false or false")

// test: OR returns true when first operand is false, second is true
function or_false_true()
    result = (false == true) or (true == true)
    assert_equal(result, true, "false or true")

// test: OR returns true when first operand is true, second is false
function or_true_false()
    result = (true == true) or (false == true)
    assert_equal(result, true, "true or false")

// test: OR returns true when both operands are true
function or_true_true()
    result = (true == true) or (true == true)
    assert_equal(result, true, "true or true")

// test: OR with multiple conditions - all false
function or_multiple_all_false()
    result = (1 > 2) or (3 > 4) or (5 > 6) or (7 > 8)
    assert_equal(result, false, "multiple or all false")

// test: OR with multiple conditions - one true in middle
function or_multiple_middle_true()
    result = (1 > 2) or (3 < 4) or (5 > 6) or (7 > 8)
    assert_equal(result, true, "multiple or middle true")

// test: OR with multiple conditions - last true
function or_multiple_last_true()
    result = (1 > 2) or (3 > 4) or (5 > 6) or (7 < 8)
    assert_equal(result, true, "multiple or last true")

// test: OR with arithmetic comparisons
function or_arithmetic_comparisons()
    x = 10
    y = 5
    z = 20
    result = ((x + y) == z) or ((x - y) == 3) or ((x * y) == 50)
    assert_equal(result, true, "arithmetic comparisons with or")

// test: OR with equality and inequality mixed
function or_equality_inequality()
    name = "Bob"
    age = 25
    result = (name == "Alice") or (age == 30) or (age > 20)
    assert_equal(result, true, "mixed equality inequality with or")

// test: OR with NOT operator
function or_with_not()
    result = (not (5 > 10)) or (not (3 == 3)) or (10 < 1)
    assert_equal(result, true, "or with not operators")

// test: OR returns false when all NOT conditions make true comparisons false
function or_not_all_false()
    result = (not (5 > 1)) or (not (10 > 5)) or (not (20 > 15))
    assert_equal(result, false, "or with not all false")

// test: OR with nested logical expressions
function or_nested_expressions()
    result = ((5 > 3) or (10 > 5)) or ((20 > 15) or (30 > 25))
    assert_equal(result, true, "nested or expressions")

// test: OR with nested mixed AND expressions
function or_nested_mixed()
    result = ((5 > 3) and (10 < 5)) or ((20 < 15) and (30 > 25))
    assert_equal(result, false, "nested mixed or with and false")

// test: OR with string comparisons
function or_string_comparisons()
    first = "apple"
    second = "banana"
    result = (first == "orange") or (second == "banana") or (first == second)
    assert_equal(result, true, "string comparisons with or")

// test: OR with type checks
function or_type_checks()
    num = 42
    text = "hello"
    result = (type(num) == "string") or (type(text) == "number") or (type(num) == "number")
    assert_equal(result, true, "type checks with or")

// test: OR with none comparisons
function or_none_comparisons()
    value = none
    other = "text"
    result = (value != none) or (other == none) or (value == none)
    assert_equal(result, true, "none comparisons with or")

// test: OR returns false when neither value is none
function or_none_makes_false()
    value = "not none"
    other = "also not none"
    result = (value == none) or (other == none)
    assert_equal(result, false, "none comparison false")

// test: OR with mixed number ranges
function or_number_ranges()
    temperature = 25
    result = (temperature < 0) or (temperature > 100) or ((temperature >= 20) and (temperature <= 30))
    assert_equal(result, true, "mixed number ranges with or")

// test: OR with boolean variable comparisons
function or_boolean_variables()
    is_active = true
    is_admin = false
    has_access = (is_active == true) or (is_admin == true)
    assert_equal(has_access, true, "boolean variables with or")

// execute all logical OR test cases
or_false_false()
or_false_true()
or_true_false()
or_true_true()
or_multiple_all_false()
or_multiple_middle_true()
or_multiple_last_true()
or_arithmetic_comparisons()
or_equality_inequality()
or_with_not()
or_not_all_false()
or_nested_expressions()
or_nested_mixed()
or_string_comparisons()
or_type_checks()
or_none_comparisons()
or_none_makes_false()
or_number_ranges()
or_boolean_variables()

// report completion of logical OR tests
os.output("[PASS] conditionals/logical/or.apex")