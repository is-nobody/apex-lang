import os

function test_no_params()
    function say_hello()
        return "Hello!"
    
    result = say_hello()
    os.output("say_hello() = {result}, expected Hello!")

function test_single_param()
    function double(x)
        return x * 2
    
    result = double(5)
    os.output("double(5) = {result}, expected 10")
    
    result = double(0)
    os.output("double(0) = {result}, expected 0")
    
    result = double(-3)
    os.output("double(-3) = {result}, expected -6")

function test_multiple_params()
    function add(a, b)
        return a + b
    
    result = add(5, 3)
    os.output("add(5, 3) = {result}, expected 8")
    
    result = add(-2, 7)
    os.output("add(-2, 7) = {result}, expected 5")
    
    result = add(0, 0)
    os.output("add(0, 0) = {result}, expected 0")

function test_string_params()
    function greet(name)
        return "Hello, {name}!"
    
    result = greet("Alice")
    os.output("greet("Alice") = {result}, expected Hello, Alice!")
    
    result = greet("")
    os.output("greet("") = {result}, expected Hello, !")

function test_no_return()
    function do_nothing()
        x = 42
    
    result = do_nothing()
    os.output("do_nothing() = {result}, expected false")

function test_nested_calls()
    function multiply(a, b)
        return a * b
    
    function add(a, b)
        return a + b
    
    result = add(5, multiply(2, 3))
    os.output("add(5, multiply(2, 3)) = {result}, expected 11")
    
    result = multiply(add(2, 3), add(1, 4))
    os.output("multiply(add(2, 3), add(1, 4)) = {result}, expected 25")

function test_function_chain()
    function get_discount(price)
        return price * 0.9
    
    function calculate_total(price, quantity)
        total = price * quantity
        return get_discount(total)
    
    result = calculate_total(100, 3)
    os.output("calculate_total(100, 3) = {result}, expected 270")
    
    result = calculate_total(50, 2)
    os.output("calculate_total(50, 2) = {result}, expected 90")

function test_boolean_params()
    function is_adult(age)
        return age >= 18
    
    result = is_adult(20)
    os.output("is_adult(20) = {result}, expected true")
    
    result = is_adult(15)
    os.output("is_adult(15) = {result}, expected false")
    
    result = is_adult(18)
    os.output("is_adult(18) = {result}, expected true")

function test_scope()
    x = 10
    
    function modify_x()
        x = 20
        return x
    
    result = modify_x()
    os.output("After modify_x(): result = {result}, x = {x}")

function test_many_params()
    function sum_five(a, b, c, d, e)
        return a + b + c + d + e
    
    result = sum_five(1, 2, 3, 4, 5)
    os.output("sum_five(1, 2, 3, 4, 5) = {result}, expected 15")

function test_recursion()
    function factorial(n)
        if n <= 1
            return 1
        else
            return n * factorial(n - 1)
    
    result = factorial(5)
    os.output("factorial(5) = {result}, expected 120")
    
    result = factorial(1)
    os.output("factorial(1) = {result}, expected 1")
    
    result = factorial(0)
    os.output("factorial(0) = {result}, expected 1")

function test_return_table()
    function create_user(name, age)
        return (name = name, age = age, active = true)
    
    user = create_user("Bob", 25)
    os.output("create_user("Bob", 25).name = {user.name}, expected Bob")
    os.output("create_user("Bob", 25).age = {user.age}, expected 25")
    os.output("create_user("Bob", 25).active = {user.active}, expected true")

function test_early_return()
    function abs_value(x)
        if x < 0
            return -x
        return x
    
    result = abs_value(-5)
    os.output("abs_value(-5) = {result}, expected 5")
    
    result = abs_value(5)
    os.output("abs_value(5) = {result}, expected 5")
    
    result = abs_value(0)
    os.output("abs_value(0) = {result}, expected 0")

test_no_params()
test_single_param()
test_multiple_params()
test_string_params()
test_no_return()
test_nested_calls()
test_function_chain()
test_boolean_params()
test_scope()
test_many_params()
test_recursion()
test_return_table()
test_early_return()