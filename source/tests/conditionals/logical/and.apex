// source/tests/conditionals/logical/and.apex
// Implementation of Logical AND Operator Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: AND returns false when both operands are false
function and_false_false()
    result = (false == true) and (false == true)
    assert_equal(result, false, "false and false")

// test: AND returns false when first operand is false, second is true
function and_false_true()
    result = (false == true) and (true == true)
    assert_equal(result, false, "false and true")

// test: AND returns false when first operand is true, second is false
function and_true_false()
    result = (true == true) and (false == true)
    assert_equal(result, false, "true and false")

// test: AND returns true only when both operands are true
function and_true_true()
    result = (true == true) and (true == true)
    assert_equal(result, true, "true and true")

// test: AND with multiple conditions - all true
function and_multiple_true()
    result = (1 < 2) and (3 < 4) and (5 < 6) and (7 < 8)
    assert_equal(result, true, "multiple and all true")

// test: AND with multiple conditions - one false in middle
function and_multiple_middle_false()
    result = (1 < 2) and (3 > 4) and (5 < 6) and (7 < 8)
    assert_equal(result, false, "multiple and middle false")

// test: AND with multiple conditions - last false
function and_multiple_last_false()
    result = (1 < 2) and (3 < 4) and (5 < 6) and (7 > 8)
    assert_equal(result, false, "multiple and last false")

// test: AND with arithmetic comparisons
function and_arithmetic_comparisons()
    x = 10
    y = 5
    z = 15
    result = ((x + y) == z) and ((x - y) == y) and ((x * y) == 50)
    assert_equal(result, true, "arithmetic comparisons with and")

// test: AND with equality and inequality mixed
function and_equality_inequality()
    name = "Alice"
    age = 30
    result = (name == "Alice") and (age != 25) and (age > 18)
    assert_equal(result, true, "mixed equality inequality with and")

// test: AND with NOT operator
function and_with_not()
    result = (not (5 > 10)) and (not (3 == 4)) and (10 > 1)
    assert_equal(result, true, "and with not operators")

// test: AND returns false when NOT makes first condition false
function and_not_makes_false()
    result = (not (5 > 1)) and (10 > 1)
    assert_equal(result, false, "and with not false first")

// test: AND with nested logical expressions
function and_nested_expressions()
    result = ((5 > 3) and (10 > 5)) and ((20 > 15) and (30 > 25))
    assert_equal(result, true, "nested and expressions")

// test: AND with string comparisons
function and_string_comparisons()
    first = "apple"
    second = "banana"
    result = (first == "apple") and (second == "banana") and (first != second)
    assert_equal(result, true, "string comparisons with and")

// test: AND with type checks
function and_type_checks()
    num = 42
    text = "hello"
    result = (type(num) == "number") and (type(text) == "string") and (num > 40)
    assert_equal(result, true, "type checks with and")

// test: AND with none comparisons
function and_none_comparisons()
    value = none
    other = "text"
    result = (value == none) and (other != none)
    assert_equal(result, true, "none comparisons with and")

// test: AND with false from none comparison
function and_none_makes_false()
    value = "not none"
    result = (value == none) and (5 > 1)
    assert_equal(result, false, "none comparison false")

// execute all logical AND test cases
and_false_false()
and_false_true()
and_true_false()
and_true_true()
and_multiple_true()
and_multiple_middle_false()
and_multiple_last_false()
and_arithmetic_comparisons()
and_equality_inequality()
and_with_not()
and_not_makes_false()
and_nested_expressions()
and_string_comparisons()
and_type_checks()
and_none_comparisons()
and_none_makes_false()

// report completion of logical AND tests
os.output("[PASS] conditionals/logical/and.apex")