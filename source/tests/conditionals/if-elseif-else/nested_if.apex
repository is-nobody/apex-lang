// source/tests/conditionals/if-elseif-else/nested_if.apex
// Implementation of Nested If Conditional Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: outer if false, inner if skipped, else branch executes
function nested_if_below()
    result = ""
    x = 5
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    assert_equal(result, "below", "x = 5")

// test: outer if true, inner if true gives "between"
function nested_if_between()
    result = ""
    x = 15
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    assert_equal(result, "between", "x = 15")

// test: outer if true, inner if false gives "above"
function nested_if_above()
    result = ""
    x = 25
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    assert_equal(result, "above", "x = 25")

// test: boundary at outer if condition (x == 10) triggers else
function nested_if_boundary_lower()
    result = ""
    x = 10
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    assert_equal(result, "below", "x = 10")

// test: boundary at inner if condition (x == 20) triggers inner else
function nested_if_boundary_inner()
    result = ""
    x = 20
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    assert_equal(result, "above", "x = 20")

// test: nested if with negative numbers, outer false case
function nested_if_negative_below()
    result = ""
    x = -15
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    assert_equal(result, "negative_below", "x = -15")

// test: nested if with negative numbers, both conditions true
function nested_if_negative_between()
    result = ""
    x = -5
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    assert_equal(result, "negative_between", "x = -5")

// test: nested if with negative numbers, outer true inner false
function nested_if_negative_above()
    result = ""
    x = 5
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    assert_equal(result, "negative_above", "x = 5")

// test: triple nested if, all conditions true
function nested_if_triple()
    result = ""
    x = 15
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "nested", "x = 15, y = 25")

// test: triple nested if, outer condition false
function nested_if_triple_x_small()
    result = ""
    x = 5
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "x_small", "x = 5, y = 25")

// test: triple nested if, middle condition false
function nested_if_triple_y_small()
    result = ""
    x = 15
    y = 15
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "y_small", "x = 15, y = 15")

// test: triple nested if, inner condition false
function nested_if_triple_not_nested()
    result = ""
    x = 25
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "not_nested", "x = 25, y = 25")

// test: triple nested if, outer boundary (x == 10)
function nested_if_triple_boundary_x()
    result = ""
    x = 10
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "x_small", "x = 10, y = 25")

// test: triple nested if, middle boundary (y == 20)
function nested_if_triple_boundary_y()
    result = ""
    x = 15
    y = 20
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "y_small", "x = 15, y = 20")

// test: triple nested if, inner boundary (x == y)
function nested_if_triple_boundary_equal()
    result = ""
    x = 30
    y = 30
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    assert_equal(result, "not_nested", "x = 30, y = 30")

// test: triple nested if with negative numbers, all conditions true
function nested_if_triple_negative()
    result = ""
    x = -5
    y = -15
    if x > -10
        if y > -20
            if x > y
                result = "neg_nested"
            else
                result = "neg_not_nested"
        else
            result = "neg_y_small"
    else
        result = "neg_x_small"
    assert_equal(result, "neg_nested", "x = -5, y = -15")

// execute all nested if test cases
nested_if_below()
nested_if_between()
nested_if_above()
nested_if_boundary_lower()
nested_if_boundary_inner()
nested_if_negative_below()
nested_if_negative_between()
nested_if_negative_above()
nested_if_triple()
nested_if_triple_x_small()
nested_if_triple_y_small()
nested_if_triple_not_nested()
nested_if_triple_boundary_x()
nested_if_triple_boundary_y()
nested_if_triple_boundary_equal()
nested_if_triple_negative()

// report completion of nested if tests
os.output("[PASS] conditionals/if-elseif-else/nested_if.apex")