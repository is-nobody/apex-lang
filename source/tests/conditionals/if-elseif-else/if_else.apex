// source/tests/conditionals/if-elseif-else/if_else.apex
// Implementation of If/Else Conditional Branch Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

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

// execute all if-else test cases
if_else_true()
if_else_false()
if_else_boundary_equal()
if_else_negative_true()
if_else_negative_false()
if_else_negative_boundary_equal()

// report completion of if-else tests
os.output("[PASS] conditionals/if-elseif-else/if_else.apex")