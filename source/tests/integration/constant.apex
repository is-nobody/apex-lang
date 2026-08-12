// source/tests/integration/constant.apex
// Implementation of Constant Variable Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: checks that code executes without errors
function assert_no_error(result, name)
    if result == none
        os.output("[ERROR] {name}: unexpected none result")

// test: basic constant declaration and access
function basic_constant()
    constant x = 42
    assert_equal(x, 42, "basic constant value")

// test: constant with string value
function string_constant()
    constant name = "Apex"
    assert_equal(name, "Apex", "string constant value")
    
    constant greeting = "Hello, World!"
    assert_equal(greeting, "Hello, World!", "string constant greeting")

// test: constant with boolean values
function boolean_constant()
    constant is_active = true
    assert_equal(is_active, true, "boolean constant true")
    
    constant is_deleted = false
    assert_equal(is_deleted, false, "boolean constant false")

// test: constant with none value
function none_constant()
    constant nothing = none
    assert_equal(nothing, none, "none constant value")

// test: constant with negative number
function negative_constant()
    constant temp = -15
    assert_equal(temp, -15, "negative constant value")
    
    constant zero = 0
    assert_equal(zero, 0, "zero constant value")

// test: constant with floating point
function float_constant()
    constant pi = 3.14159
    assert_equal(pi > 3.14, true, "float constant pi > 3.14")
    assert_equal(pi < 3.15, true, "float constant pi < 3.15")
    
    constant e = 2.71828
    assert_equal(e > 2.7, true, "float constant e > 2.7")

// test: constant used in expressions
function constant_in_expression()
    constant base = 10
    constant multiplier = 5
    result = base * multiplier + 2
    assert_equal(result, 52, "constant in expression")

// test: multiple constants in scope
function multiple_constants()
    constant a = 1
    constant b = 2
    constant c = 3
    sum = a + b + c
    assert_equal(sum, 6, "sum of three constants")

// test: constant with table value
function table_constant()
    constant colors = ["red", "green", "blue"]
    assert_equal(colors[1], "red", "constant table[1]")
    assert_equal(colors[2], "green", "constant table[2]")
    assert_equal(colors[3], "blue", "constant table[3]")

// test: constant with key-value table
function key_value_constant()
    constant config = ["host" = "localhost", "port" = 8080]
    assert_equal(config["host"], "localhost", "constant table host")
    assert_equal(config["port"], 8080, "constant table port")

// test: constant in function scope
function constant_in_function()
    function calculate(radius)
        constant pi = 3.14159
        return pi * radius * radius
    
    area = calculate(5)
    assert_equal(area > 78.5, true, "constant in function area > 78.5")
    assert_equal(area < 78.6, true, "constant in function area < 78.6")

// test: constants in for loop scope
function constant_in_for_loop()
    sum = 0
    for i = 1, 5
        constant multiplier = 10
        sum = sum + i * multiplier
    assert_equal(sum, 150, "constant in for loop scope")

// test: constant shadowing in nested scope
function constant_shadowing()
    constant x = 100
    assert_equal(x, 100, "outer constant x")
    
    function inner()
        constant x = 200
        assert_equal(x, 200, "inner constant x (shadowed)")
        return x
    
    result = inner()
    assert_equal(result, 200, "inner function returned shadowed constant")
    assert_equal(x, 100, "outer constant unchanged after shadowing")

// test: constant in if block (different scopes for if/else)
function constant_in_if_else()
    condition = true
    if condition == true
        constant msg_true = "Condition is true"
        assert_equal(msg_true, "Condition is true", "constant in if block")
    else
        constant msg_false = "Condition is false"
        assert_equal(msg_false, "Condition is false", "constant in else block")

// test: constant in elif chain
function constant_in_elif()
    value = 2
    if value == 1
        constant result_one = "one"
        assert_equal(result_one, "one", "constant result one")
    elif value == 2
        constant result_two = "two"
        assert_equal(result_two, "two", "constant result two")
    else
        constant result_other = "other"
        assert_equal(result_other, "other", "constant result other")

// test: constant used as table value (not key)
function constant_as_table_value()
    constant name_value = "Alice"
    constant age_value = 30
    
    user = ["name" = name_value, "age" = age_value]
    assert_equal(user["name"], "Alice", "constant as table value name")
    assert_equal(user["age"], 30, "constant as table value age")

// test: constant from function return
function constant_from_function()
    function get_default_port()
        return 8080
    
    constant port = get_default_port()
    assert_equal(port, 8080, "constant from function return")

// test: constant used in ternary expression
function constant_in_ternary()
    constant threshold = 50
    value = 75
    result = value if value > threshold else "low"
    assert_equal(result, 75, "constant in ternary true")
    
    low_value = 25
    result2 = low_value if low_value > threshold else "low"
    assert_equal(result2, "low", "constant in ternary false")

// test: constant with string interpolation
function constant_in_interpolation()
    constant name = "World"
    greeting = "Hello, {name}!"
    assert_equal(greeting, "Hello, World!", "constant in interpolation")

// test: constant in nested for loops
function constant_in_nested_loops()
    constant factor = 10
    matrix = []
    for i = 1, 3
        matrix[i] = []
        for j = 1, 3
            matrix[i][j] = i * factor + j
    
    assert_equal(matrix[1][1], 11, "nested loop constant 1,1")
    assert_equal(matrix[2][2], 22, "nested loop constant 2,2")
    assert_equal(matrix[3][3], 33, "nested loop constant 3,3")

// test: constant used with comparison operators
function constant_comparison()
    constant MAX_SIZE = 100
    assert_equal(50 < MAX_SIZE, true, "50 < MAX_SIZE")
    assert_equal(150 > MAX_SIZE, true, "150 > MAX_SIZE")
    assert_equal(MAX_SIZE == 100, true, "MAX_SIZE == 100")
    assert_equal(MAX_SIZE != 50, true, "MAX_SIZE != 50")

// test: constant with logical operations (using explicit comparisons)
function constant_logical()
    constant TRUE_VAL = true
    constant FALSE_VAL = false
    assert_equal(TRUE_VAL == true and TRUE_VAL == true, true, "true and true")
    assert_equal(TRUE_VAL == true or FALSE_VAL == true, true, "true or false")
    assert_equal(not FALSE_VAL, true, "not false")
    assert_equal(not TRUE_VAL, false, "not true")

// test: constant used in array index
function constant_array_index()
    constant INDEX = 2
    data = [10, 20, 30, 40, 50]
    assert_equal(data[INDEX], 20, "constant as array index")

// test: many constants in single scope
function many_constants()
    constant v1 = 1
    constant v2 = 2
    constant v3 = 3
    constant v4 = 4
    constant v5 = 5
    constant v6 = 6
    constant v7 = 7
    constant v8 = 8
    constant v9 = 9
    constant v10 = 10
    
    total = v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10
    assert_equal(total, 55, "sum of 10 constants")

// test: constant in return statement
function constant_in_return()
    function get_max_value()
        constant MAX = 999
        return MAX
    
    assert_equal(get_max_value(), 999, "constant returned from function")

// test: constant with complex expression initialization
function constant_complex_init()
    constant result = (10 + 5) * 2 - 3
    assert_equal(result, 27, "constant with complex expression init")

// test: constant referencing another constant
function constant_reference()
    constant base = 50
    constant doubled = base * 2
    constant quadrupled = doubled * 2
    assert_equal(doubled, 100, "constant referencing constant doubled")
    assert_equal(quadrupled, 200, "constant referencing constant quadrupled")

// test: constant scope isolation in functions
function constant_scope_isolation()
    constant x = "outer"
    
    function test_scope()
        constant x = "inner"
        return x
    
    inner_result = test_scope()
    assert_equal(inner_result, "inner", "inner scope constant")
    assert_equal(x, "outer", "outer scope constant unchanged")

// test: constant used with arithmetic operators
function constant_arithmetic()
    constant A = 100
    constant B = 7
    
    assert_equal(A + B, 107, "constant addition")
    assert_equal(A - B, 93, "constant subtraction")
    assert_equal(A * B, 700, "constant multiplication")
    assert_equal(A / B > 14.28, true, "constant division")
    assert_equal(A / B < 14.29, true, "constant division bound")
    assert_equal(A % B, 2, "constant modulo")

// test: constant with unary minus
function constant_unary_minus()
    constant pos = 42
    negated = -pos
    assert_equal(negated, -42, "constant unary minus")
    
    constant neg = -10
    abs_val = -neg
    assert_equal(abs_val, 10, "constant double negation")

// test: constant in for loop condition
function constant_in_loop_condition()
    constant LIMIT = 5
    count = 0
    for i = 1, LIMIT
        count = count + 1
    assert_equal(count, LIMIT, "constant as loop limit")

// test: constant with scientific notation
function constant_scientific()
    constant avogadro = 6.022e23
    assert_equal(avogadro > 6.0e23, true, "scientific notation constant >")
    assert_equal(avogadro < 6.1e23, true, "scientific notation constant <")

// test: constant used in table value construction
function constant_in_table_construction()
    constant DEFAULT_NAME = "Anonymous"
    constant DEFAULT_AGE = 0
    
    user = ["name" = DEFAULT_NAME, "age" = DEFAULT_AGE]
    assert_equal(user["name"], "Anonymous", "constant in table value")
    assert_equal(user["age"], 0, "constant in table value")

// test: nested function constants
function nested_function_constants()
    function outer_func()
        constant outer_val = "outer"
        
        function inner_func()
            constant inner_val = "inner"
            return "{outer_val} {inner_val}"
        
        return inner_func()
    
    result = outer_func()
    assert_equal(result, "outer inner", "nested function constants")

// test: constant with modulo operations
function constant_modulo()
    constant DIVISOR = 3
    assert_equal(10 % DIVISOR, 1, "10 % 3")
    assert_equal(9 % DIVISOR, 0, "9 % 3")
    assert_equal(11 % DIVISOR, 2, "11 % 3")

// test: constant type checking
function constant_type_check()
    constant num = 42
    constant str = "hello"
    constant bool_val = true
    constant none_val = none
    
    assert_equal(type(num), "number", "constant number type")
    assert_equal(type(str), "string", "constant string type")
    assert_equal(type(bool_val), "boolean", "constant boolean type")
    assert_equal(type(none_val), "none", "constant none type")

// test: constant with large numbers
function constant_large_numbers()
    constant MILLION = 1000000
    constant BILLION = 1000000000
    
    assert_equal(MILLION * 1000, BILLION, "million * 1000 == billion")
    assert_equal(BILLION / MILLION, 1000, "billion / million == 1000")

// test: constant in complex table operations
function constant_table_operations()
    constant START = 1
    constant END = 10
    constant MULTIPLIER = 5
    
    values = []
    for i = START, END
        values[i] = i * MULTIPLIER
    
    sum = 0
    for i = START, END
        sum = sum + values[i]
    
    assert_equal(sum, 275, "constant in complex table operations")

// execute all constant test cases
basic_constant()
string_constant()
boolean_constant()
none_constant()
negative_constant()
float_constant()
constant_in_expression()
multiple_constants()
table_constant()
key_value_constant()
constant_in_function()
constant_in_for_loop()
constant_shadowing()
constant_in_if_else()
constant_in_elif()
constant_as_table_value()
constant_from_function()
constant_in_ternary()
constant_in_interpolation()
constant_in_nested_loops()
constant_comparison()
constant_logical()
constant_array_index()
many_constants()
constant_in_return()
constant_complex_init()
constant_reference()
constant_scope_isolation()
constant_arithmetic()
constant_unary_minus()
constant_in_loop_condition()
constant_scientific()
constant_in_table_construction()
nested_function_constants()
constant_modulo()
constant_type_check()
constant_large_numbers()
constant_table_operations()

// report completion of constant integration tests
os.output("[PASS] integration/constant.apex")