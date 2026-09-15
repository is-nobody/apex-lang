// source/tests/integration/if_else.apex
// Combined Implementation of Conditional Branch Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: if block executes when condition is true
function if_single_true()
    result = "small"
    x = 10
    if x > 5
        result = "big"
    assert_equal(result, "big", "x = 10, x > 5 = true")

// test: if block skipped when condition is false, variable unchanged
function if_single_false()
    result = "small"
    x = 3
    if x > 5
        result = "big"
    assert_equal(result, "small", "x = 3, x > 5 = false")

// test: negative numbers, if block executes when condition holds
function if_single_negative_true()
    result = "less"
    x = -3
    if x > -5
        result = "greater"
    assert_equal(result, "greater", "x = -3, x > -5 = true")

// test: negative numbers, if block skipped when condition fails
function if_single_negative_false()
    result = "less"
    x = -10
    if x > -5
        result = "greater"
    assert_equal(result, "less", "x = -10, x > -5 = false")

// test: if branch executes when condition is true
function if_else_true()
    result = ""
    age = 25
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "adult", "age = 25")

// test: else branch executes when condition is false
function if_else_false()
    result = ""
    age = 16
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "minor", "age = 16")

// test: boundary condition (age == 18) triggers if branch
function if_else_boundary_equal()
    result = ""
    age = 18
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "adult", "age = 18")

// test: negative numbers, if branch executes when condition holds
function if_else_negative_true()
    result = ""
    temp = -5
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "warm", "temp = -5")

// test: negative numbers, else branch executes when condition fails
function if_else_negative_false()
    result = ""
    temp = -15
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "cold", "temp = -15")

// test: negative boundary condition (temp == -10) triggers if branch
function if_else_negative_boundary_equal()
    result = ""
    temp = -10
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "warm", "temp = -10")

// test: first condition matches (score >= 90)
function if_elseif_else_first()
    result = ""
    score = 95
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "A", "score = 95")

// test: middle condition matches (score >= 80)
function if_elseif_else_middle()
    result = ""
    score = 85
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "B", "score = 85")

// test: last elseif condition matches (score >= 70)
function if_elseif_else_last()
    result = ""
    score = 75
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "C", "score = 75")

// test: else branch executes when no conditions match
function if_elseif_else_else()
    result = ""
    score = 65
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "F", "score = 65")

// test: boundary condition for first branch (score == 90)
function if_elseif_else_boundary_first()
    result = ""
    score = 90
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "A", "score = 90")

// test: boundary condition for middle branch (score == 80)
function if_elseif_else_boundary_middle()
    result = ""
    score = 80
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "B", "score = 80")

// test: boundary condition for last branch (score == 70)
function if_elseif_else_boundary_last()
    result = ""
    score = 70
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "C", "score = 70")

// test: negative score falls through to else branch
function if_elseif_else_negative_all()
    result = ""
    score = -10
    if score >= 90
        result = "A"
    else if score >= 80
        result = "B"
    else if score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "F", "score = -10")

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

// execute all test cases
if_single_true()
if_single_false()
if_single_negative_true()
if_single_negative_false()
if_else_true()
if_else_false()
if_else_boundary_equal()
if_else_negative_true()
if_else_negative_false()
if_else_negative_boundary_equal()
if_elseif_else_first()
if_elseif_else_middle()
if_elseif_else_last()
if_elseif_else_else()
if_elseif_else_boundary_first()
if_elseif_else_boundary_middle()
if_elseif_else_boundary_last()
if_elseif_else_negative_all()
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

// report completion of all conditional tests
os.output("[PASS] integration/if_else.apex")