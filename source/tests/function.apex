import os

errors = 0

// function with output (no return)
function greet(name)
    os.output("Hello, {name}!")

// function with return value
function add(a, b)
    return a + b

result = add(5, 3)
expected = 8
if result != expected
    errors = errors + 1
    os.output("  ERROR: add(5, 3) expected {expected}, got {result}")

result = add(10, -2)
expected = 8
if result != expected
    errors = errors + 1
    os.output("  ERROR: add(10, -2) expected {expected}, got {result}")

result = add(0, 0)
expected = 0
if result != expected
    errors = errors + 1
    os.output("  ERROR: add(0, 0) expected {expected}, got {result}")

// function with conditional logic
function check_age(age)
    if age < 18
        return "Too young"
    return "Welcome"

result = check_age(15)
expected = "Too young"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_age(15) expected '{expected}', got '{result}'")

result = check_age(18)
expected = "Welcome"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_age(18) expected '{expected}', got '{result}'")

result = check_age(25)
expected = "Welcome"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_age(25) expected '{expected}', got '{result}'")

result = check_age(0)
expected = "Too young"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_age(0) expected '{expected}', got '{result}'")

// function with multiple parameters
function multiply(x, y)
    return x * y

result = multiply(4, 5)
expected = 20
if result != expected
    errors = errors + 1
    os.output("  ERROR: multiply(4, 5) expected {expected}, got {result}")

result = multiply(7, 6)
expected = 42
if result != expected
    errors = errors + 1
    os.output("  ERROR: multiply(7, 6) expected {expected}, got {result}")

// function with string concatenation
function full_name(first, last)
    return "{first} {last}"

result = full_name("Alice", "Smith")
expected = "Alice Smith"
if result != expected
    errors = errors + 1
    os.output("  ERROR: full_name('Alice', 'Smith') expected '{expected}', got '{result}'")

result = full_name("Bob", "Jones")
expected = "Bob Jones"
if result != expected
    errors = errors + 1
    os.output("  ERROR: full_name('Bob', 'Jones') expected '{expected}', got '{result}'")

// function with no return value (returns none)
function do_nothing()
    // no return

result = do_nothing()
expected = none
if result != expected
    errors = errors + 1
    os.output("  ERROR: do_nothing() expected {expected}, got {result}")

// nested function calls
function square(x)
    return x * x

function cube(x)
    return x * square(x)

result = cube(3)
expected = 27
if result != expected
    errors = errors + 1
    os.output("  ERROR: cube(3) expected {expected}, got {result}")

result = cube(4)
expected = 64
if result != expected
    errors = errors + 1
    os.output("  ERROR: cube(4) expected {expected}, got {result}")

// function call in expression
function double(x)
    return x * 2

result = double(5) + double(3)
expected = 16
if result != expected
    errors = errors + 1
    os.output("  ERROR: double(5) + double(3) expected {expected}, got {result}")

// function with table parameter
function first_item(table)
    return table.1

colors = ("red", "green", "blue")
result = first_item(colors)
expected = "red"
if result != expected
    errors = errors + 1
    os.output("  ERROR: first_item(colors) expected '{expected}', got '{result}'")

numbers = (10, 20, 30)
result = first_item(numbers)
expected = 10
if result != expected
    errors = errors + 1
    os.output("  ERROR: first_item(numbers) expected {expected}, got {result}")

// function that modifies table
function add_item(table, item)
    table.1 = item
    return table

colors = ("red", "green")
modified = add_item(colors, "blue")
result = modified.1
expected = "blue"
if result != expected
    errors = errors + 1
    os.output("  ERROR: add_item modified first item expected '{expected}', got '{result}'")

// final result
os.output("Total errors in functions.apex: {errors}")

if errors == 0
    os.output("Tests in functions.apex passed!")
else
    os.output("{errors} test(s) failed in functions.apex.")