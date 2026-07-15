import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function no_params()
    function say_hello()
        return "Hello!"
    
    result = say_hello()
    assert_equal(result, "Hello!", "say_hello()")

function single_param()
    function double(x)
        return x * 2
    
    result = double(5)
    assert_equal(result, 10, "double(5)")
    
    result = double(0)
    assert_equal(result, 0, "double(0)")
    
    result = double(-3)
    assert_equal(result, -6, "double(-3)")

function multiple_params()
    function add(a, b)
        return a + b
    
    result = add(5, 3)
    assert_equal(result, 8, "add(5, 3)")
    
    result = add(-2, 7)
    assert_equal(result, 5, "add(-2, 7)")
    
    result = add(0, 0)
    assert_equal(result, 0, "add(0, 0)")

function string_params()
    function greet(name)
        return "Hello, {name}!"
    
    result = greet("Alice")
    assert_equal(result, "Hello, Alice!", "greet('Alice')")
    
    result = greet("")
    assert_equal(result, "Hello, !", "greet('')")

function no_return()
    function do_nothing()
        x = 42
    
    result = do_nothing()
    assert_equal(result, none, "do_nothing()")

function nested_calls()
    function multiply(a, b)
        return a * b
    
    function add(a, b)
        return a + b
    
    result = add(5, multiply(2, 3))
    assert_equal(result, 11, "add(5, multiply(2, 3))")
    
    result = multiply(add(2, 3), add(1, 4))
    assert_equal(result, 25, "multiply(add(2, 3), add(1, 4))")

function function_chain()
    function get_discount(price)
        return price * 0.9
    
    function calculate_total(price, quantity)
        total = price * quantity
        return get_discount(total)
    
    result = calculate_total(100, 3)
    assert_equal(result, 270, "calculate_total(100, 3)")
    
    result = calculate_total(50, 2)
    assert_equal(result, 90, "calculate_total(50, 2)")

function boolean_params()
    function is_adult(age)
        return age >= 18
    
    result = is_adult(20)
    assert_equal(result, true, "is_adult(20)")
    
    result = is_adult(15)
    assert_equal(result, false, "is_adult(15)")
    
    result = is_adult(18)
    assert_equal(result, true, "is_adult(18)")

function many_params()
    function sum_five(a, b, c, d, e)
        return a + b + c + d + e
    
    result = sum_five(1, 2, 3, 4, 5)
    assert_equal(result, 15, "sum_five(1, 2, 3, 4, 5)")

function recursion()
    function factorial(n)
        if n <= 1
            return 1
        else
            return n * factorial(n - 1)
    
    result = factorial(5)
    assert_equal(result, 120, "factorial(5)")
    
    result = factorial(1)
    assert_equal(result, 1, "factorial(1)")
    
    result = factorial(0)
    assert_equal(result, 1, "factorial(0)")

function return_table()
    function create_user(name, age)
        return ["name" = name, "age" = age, "active" = true]
    
    user = create_user("Bob", 25)
    assert_equal(user["name"], "Bob", "create_user name")
    assert_equal(user["age"], 25, "create_user age")
    assert_equal(user["active"], true, "create_user active")

function early_return()
    function abs_value(x)
        if x < 0
            return -x
        return x
    
    result = abs_value(-5)
    assert_equal(result, 5, "abs_value(-5)")
    
    result = abs_value(5)
    assert_equal(result, 5, "abs_value(5)")
    
    result = abs_value(0)
    assert_equal(result, 0, "abs_value(0)")

function test_function_equality()
    function add(a, b)
        return a + b
    
    function subtract(a, b)
        return a - b
    
    func1 = add
    func2 = add
    func3 = subtract
    
    assert_equal(func1 == func2, true, "same function ==")
    assert_equal(func1 == func3, false, "different function ==")
    assert_equal(func1 != func2, false, "same function !=")
    assert_equal(func1 != func3, true, "different function !=")
    assert_equal(add == add, true, "direct function ==")
    assert_equal(add == subtract, false, "direct function !=")

no_params()
single_param()
multiple_params()
string_params()
no_return()
nested_calls()
function_chain()
boolean_params()
many_params()
recursion()
return_table()
early_return()
test_function_equality()

if tests_failed == 0
    os.output("[OK] integration/function.apex")