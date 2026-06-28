import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function if_elif_else_first()
    result = ""
    score = 95
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "A", "score = 95")

function if_elif_else_middle()
    result = ""
    score = 85
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "B", "score = 85")

function if_elif_else_last()
    result = ""
    score = 75
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "C", "score = 75")

function if_elif_else_else()
    result = ""
    score = 65
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "F", "score = 65")

function if_elif_else_boundary_first()
    result = ""
    score = 90
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "A", "score = 90")

function if_elif_else_boundary_middle()
    result = ""
    score = 80
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "B", "score = 80")

function if_elif_else_boundary_last()
    result = ""
    score = 70
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "C", "score = 70")

function if_elif_else_negative_all()
    result = ""
    score = -10
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    assert_equal(result, "F", "score = -10")

if_elif_else_first()
if_elif_else_middle()
if_elif_else_last()
if_elif_else_else()
if_elif_else_boundary_first()
if_elif_else_boundary_middle()
if_elif_else_boundary_last()
if_elif_else_negative_all()

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/if_elif_else.apex")