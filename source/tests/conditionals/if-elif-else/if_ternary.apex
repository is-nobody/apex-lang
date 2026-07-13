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

function ternary_numeric_result()
    a = 5
    b = 10
    max_val = a if a > b else b
    assert_equal(max_val, 10, "ternary_numeric_result")

function ternary_explicit_condition()
    flag = true
    status = "Active" if flag == true else "Inactive"
    assert_equal(status, "Active", "ternary_explicit_condition")

function ternary_with_arithmetic()
    x = 7
    y = 3
    result = x + y if x > y else x - y
    assert_equal(result, 10, "ternary_with_arithmetic")

function ternary_boolean_false_case()
    is_valid = false
    result = "Valid" if is_valid == true else "Invalid"
    assert_equal(result, "Invalid", "ternary_boolean_false_case")

function ternary_with_modulo()
    x = 42
    result = "Even" if x % 2 == 0 else "Odd"
    assert_equal(result, "Even", "ternary_with_modulo")

function ternary_nested_expressions()
    a = 10
    b = 20
    sum = a + b
    result = "Large sum" if sum > 25 else "Small sum"
    assert_equal(result, "Large sum", "ternary_nested_expressions")

function ternary_equality_check()
    password = "secret123"
    input_password = "secret123"
    access = "Granted" if password == input_password else "Denied"
    assert_equal(access, "Granted", "ternary_equality_check")

function ternary_not_equals()
    value = 5
    result = "Not zero" if value != 0 else "Zero"
    assert_equal(result, "Not zero", "ternary_not_equals")

function ternary_less_than_or_equal()
    age = 18
    status = "Adult" if age >= 18 else "Minor"
    assert_equal(status, "Adult", "ternary_less_than_or_equal")

function ternary_float_comparison()
    price = 99.99
    result = "Expensive" if price > 50.0 else "Cheap"
    assert_equal(result, "Expensive", "ternary_float_comparison")

function ternary_negative_numbers()
    temp = -5
    result = "Freezing" if temp <= 0 else "Above freezing"
    assert_equal(result, "Freezing", "ternary_negative_numbers")

function ternary_multiplication_comparison()
    width = 5
    height = 3
    result = "Landscape" if width > height else "Portrait"
    assert_equal(result, "Landscape", "ternary_multiplication_comparison")

function ternary_with_literal_arrays()
    len = 5
    result = "Long" if len > 3 else "Short"
    assert_equal(result, "Long", "ternary_with_literal_arrays")

function ternary_zero_comparison()
    count = 0
    result = "Empty" if count == 0 else "Has items"
    assert_equal(result, "Empty", "ternary_zero_comparison")

function ternary_boolean_not_equals_false()
    debug = false
    result = "Production" if debug == false else "Debug"
    assert_equal(result, "Production", "ternary_boolean_not_equals_false")

ternary_basic_boolean()
ternary_variable_comparison()
ternary_string_result()
ternary_numeric_result()
ternary_explicit_condition()
ternary_with_arithmetic()
ternary_boolean_false_case()
ternary_with_modulo()
ternary_nested_expressions()
ternary_equality_check()
ternary_not_equals()
ternary_less_than_or_equal()
ternary_float_comparison()
ternary_negative_numbers()
ternary_multiplication_comparison()
ternary_with_literal_arrays()
ternary_zero_comparison()
ternary_boolean_not_equals_false()

if tests_failed == 0
    os.output("[OK] conditionals/if-elif-else/if_ternary.apex")