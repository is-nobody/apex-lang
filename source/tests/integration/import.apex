// source/tests/integration/import.apex
// Implementation of Module Import and Namespace Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import math
import lib
import lib.subfolder.math_utils
import lib.string_utils

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: basic function import from lib module
function test_basic_import()
    add_result = lib.add(5, 3)
    assert_equal(add_result, 8, "lib.add(5, 3)")

// test: module-level variables imported correctly
function test_module_variables()
    assert_equal(lib.pi, math.pi(), "lib.pi == math.pi()")
    assert_equal(lib.e, math.e(), "lib.e == math.e()")

// test: submodule function accessible via nested path
function test_submodule_function()
    circle_result = lib.subfolder.math_utils.circle_area(2)
    assert_equal(circle_result, math.pi() * 4, "lib.subfolder.math_utils.circle_area(2)")

// test: multiple nested module calls with string utilities
function test_nested_module_calls()
    reversed_text = lib.string_utils.reverse("hello")
    assert_equal(reversed_text, "olleh", "lib.string_utils.reverse('hello')")

    upper_text = lib.string_utils.to_upper("test")
    assert_equal(upper_text, "TEST", "lib.string_utils.to_upper('test')")

// test: module variable used in arithmetic expression
function test_module_var_in_expression()
    radius = 3
    volume = (4.0 / 3.0) * lib.pi * lib.subfolder.math_utils.power(radius, 3)
    expected_volume = (4.0 / 3.0) * math.pi() * (radius * radius * radius)
    assert_equal(volume, expected_volume, "sphere volume using lib.pi and lib.subfolder.math_utils.power")

// test: table storing results from various module calls
function test_table_with_module_values()
    results = [
        "add" = lib.add(10, 20),
        "multiply" = lib.multiply(7, 8),
        "circle" = lib.subfolder.math_utils.circle_area(5),
        "upper_name" = lib.string_utils.to_upper("apex"),
        "length" = lib.string_utils.get_length("hello world")
    ]

    assert_equal(results["add"], 30, "results['add']")
    assert_equal(results["multiply"], 56, "results['multiply']")
    assert_equal(results["circle"], math.pi() * 25, "results['circle']")
    assert_equal(results["upper_name"], "APEX", "results['upper_name']")
    assert_equal(results["length"], 11, "results['length']")

// test: module function called within conditional branch
function test_conditional_module_call()
    test_value = -5
    abs_result = 0
    if test_value < 0
        abs_result = lib.subtract(0, test_value)
    else
        abs_result = test_value
    assert_equal(abs_result, 5, "conditional abs using lib.subtract")

// test: factorial function from submodule
function test_factorial_module()
    fact_result = lib.subfolder.math_utils.factorial(5)
    assert_equal(fact_result, 120, "lib.subfolder.math_utils.factorial(5)")

// test: complex nested calls across multiple modules
function test_complex_nested_calls()
    x = lib.add(1, 2)
    y = lib.multiply(x, 3)
    z = lib.subfolder.math_utils.power(y, 2)
    final = lib.string_utils.get_length("test")
    assert_equal(z, 81, "complex nested: lib.subfolder.math_utils.power(lib.multiply(lib.add(1,2), 3), 2)")
    assert_equal(final, 4, "lib.string_utils.get_length('test')")

// test: chained module calls in sequence
function test_chained_module_calls()
    num1 = lib.add(5, 5)
    num2 = lib.multiply(num1, 2)
    num3 = lib.subfolder.math_utils.power(num2, 2)
    assert_equal(num3, 400, "chained module calls: power(multiply(add(5,5), 2), 2)")

// test: ternary operator with module function calls
function test_ternary_with_modules()
    a = 10
    b = 20
    max_val = a if a > b else b
    min_val = lib.subtract(a, b) if a > b else lib.subtract(b, a)
    assert_equal(max_val, 20, "ternary max")
    assert_equal(min_val, 10, "ternary abs diff using lib.subtract")

// execute all module import test cases
test_basic_import()
test_module_variables()
test_submodule_function()
test_nested_module_calls()
test_module_var_in_expression()
test_table_with_module_values()
test_conditional_module_call()
test_factorial_module()
test_complex_nested_calls()
test_chained_module_calls()
test_ternary_with_modules()

// report completion of module import tests
os.output("[PASS] integration/import.apex")