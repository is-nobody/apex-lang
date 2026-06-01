import os


// function with output (no return)
function greet(name)
    os.output("Hello, {name}!")

// function with return value
function add(a, b)
    return a + b

result = add(5, 3)
os.output("add(5, 3) = {result}, expected 8")

result = add(10, -2)
os.output("add(10, -2) = {result}, expected 8")

result = add(0, 0)
os.output("add(0, 0) = {result}, expected 0")

// function with conditional logic
function check_age(age)
    if age < 18
        return "Too young"
    return "Welcome"

result = check_age(15)
os.output("check_age(15) = {result}, expected Too young")

result = check_age(18)
os.output("check_age(18) = {result}, expected Welcome")

result = check_age(25)
os.output("check_age(25) = {result}, expected Welcome")

result = check_age(0)
os.output("check_age(0) = {result}, expected Too young")

// function with multiple parameters
function multiply(x, y)
    return x * y

result = multiply(4, 5)
os.output("multiply(4, 5) = {result}, expected 20")

result = multiply(7, 6)
os.output("multiply(7, 6) = {result}, expected 42")

// function with string concatenation
function full_name(first, last)
    return "{first} {last}"

result = full_name("Alice", "Smith")
os.output("full_name(Alice, Smith) = {result}, expected Alice Smith")

result = full_name("Bob", "Jones")
os.output("full_name(Bob, Jones) = {result}, expected Bob Jones")

// function with no return value (returns none)
function do_nothing()
    // no return

result = do_nothing()
os.output("do_nothing() = {result}, expected none")

// nested function calls
function square(x)
    return x * x

function cube(x)
    return x * square(x)

result = cube(3)
os.output("cube(3) = {result}, expected 27")

result = cube(4)
os.output("cube(4) = {result}, expected 64")

// function call in expression
function double(x)
    return x * 2

result = double(5) + double(3)
os.output("double(5) + double(3) = {result}, expected 16")

// function with table parameter
function first_item(table)
    return table.1

colors = ("red", "green", "blue")
result = first_item(colors)
os.output("first_item(colors) = {result}, expected red")

numbers = (10, 20, 30)
result = first_item(numbers)
os.output("first_item(numbers) = {result}, expected 10")

// function that modifies table
function add_item(table, item)
    table.1 = item
    return table

colors = ("red", "green")
modified = add_item(colors, "blue")
result = modified.1
os.output("modified.1 = {result}, expected blue")

// final result
