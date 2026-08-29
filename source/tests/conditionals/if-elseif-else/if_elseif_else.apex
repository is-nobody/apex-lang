// source/tests/conditionals/if-elseif-else/if_elseif_else.apex
// Implementation of If/Else-If/Else Conditional Chain Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

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

// execute all if-elseif-else test cases
if_elseif_else_first()
if_elseif_else_middle()
if_elseif_else_last()
if_elseif_else_else()
if_elseif_else_boundary_first()
if_elseif_else_boundary_middle()
if_elseif_else_boundary_last()
if_elseif_else_negative_all()

// report completion of if-elseif-else tests
os.output("[PASS] conditionals/if-elseif-else/if_elseif_else.apex")