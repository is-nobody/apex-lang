import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function ternary_basic_boolean()
    result = "Pass" if 1 == 1 else "Fail"
    assert_equal(result, "Pass", "ternary_basic_boolean")

function ternary_variable_comparison()
    x = 10
    result = "Big" if x > 5 else "Small"
    assert_equal(result, "Big", "ternary_variable_comparison")

function ternary_string_result()
    name = "Apex"
    greeting = "Hello Apex" if name == "Apex" else "Hello Stranger"
    assert_equal(greeting, "Hello Apex", "ternary_string_result")

function ternary_nested()
    val = 15
    category = "Low" if val < 10 else "Medium" if val < 20 else "High"
    assert_equal(category, "Medium", "ternary_nested")

function ternary_numeric_result()
    a = 5
    b = 10
    max_val = a if a > b else b
    assert_equal(max_val, 10, "ternary_numeric_result")

function ternary_explicit_condition()
    flag = true
    status = "Active" if flag == true else "Inactive"
    assert_equal(status, "Active", "ternary_explicit_condition")

ternary_basic_boolean()
ternary_variable_comparison()
ternary_string_result()
ternary_nested()
ternary_numeric_result()
ternary_explicit_condition()

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/if_ternary.apex")