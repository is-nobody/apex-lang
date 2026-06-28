// if_else.apex
import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function if_else_true()
    result = ""
    age = 25
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "adult", "age = 25")

function if_else_false()
    result = ""
    age = 16
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "minor", "age = 16")

function if_else_boundary_equal()
    result = ""
    age = 18
    if age >= 18
        result = "adult"
    else
        result = "minor"
    assert_equal(result, "adult", "age = 18")

function if_else_negative_true()
    result = ""
    temp = -5
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "warm", "temp = -5")

function if_else_negative_false()
    result = ""
    temp = -15
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "cold", "temp = -15")

function if_else_negative_boundary_equal()
    result = ""
    temp = -10
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    assert_equal(result, "warm", "temp = -10")

if_else_true()
if_else_false()
if_else_boundary_equal()
if_else_negative_true()
if_else_negative_false()
if_else_negative_boundary_equal()

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/if_else.apex")