import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

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

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/nested_if.apex")