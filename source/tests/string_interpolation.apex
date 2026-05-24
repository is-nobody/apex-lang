import os

errors = 0
result = ""

// ==========================================
// SECTION 1: Basic Interpolation
// ==========================================

name = "Alice"
age = 30
result = "Hello, {name}!"
if result != "Hello, Alice!"
    os.output("  ERROR: expected 'Hello, Alice!', got '{result}'")
    errors = errors + 1

result = "{name} is {age} years old"
if result != "Alice is 30 years old"
    os.output("  ERROR: expected 'Alice is 30 years old', got '{result}'")
    errors = errors + 1

// none in interpolation
x = none
result = "x is {x}"
if result != "x is none"
    os.output("  ERROR: expected 'x is none', got '{result}'")
    errors = errors + 1

// booleans in interpolation
is_active = true
result = "Status: {is_active}"
if result != "Status: True"
    os.output("  ERROR: expected 'Status: True', got '{result}'")
    errors = errors + 1

is_active = false
result = "Status: {is_active}"
if result != "Status: False"
    os.output("  ERROR: expected 'Status: False', got '{result}'")
    errors = errors + 1

// numbers in interpolation
score = 95
price = 19.99
result = "Score: {score}, Price: {price}"
if result != "Score: 95, Price: 19.99"
    os.output("  ERROR: expected 'Score: 95, Price: 19.99', got '{result}'")
    errors = errors + 1

// concatenation via interpolation
first = "Hello"
second = "World"
result = "{first}, {second}!"
if result != "Hello, World!"
    os.output("  ERROR: expected 'Hello, World!', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 2: Variable Type Changes in Interpolation
// ==========================================

x_change = 10
result = "x was {x_change}"
if result != "x was 10"
    os.output("  ERROR: expected 'x was 10', got '{result}'")
    errors = errors + 1

x_change = "string"
result = "x became {x_change}"
if result != "x became string"
    os.output("  ERROR: expected 'x became string', got '{result}'")
    errors = errors + 1

x_change = true
result = "x became {x_change}"
if result != "x became True"
    os.output("  ERROR: expected 'x became True', got '{result}'")
    errors = errors + 1

x_change = none
result = "x became {x_change}"
if result != "x became none"
    os.output("  ERROR: expected 'x became none', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 3: Math Expressions in Interpolation
// ==========================================

count = 5
price = 10.5

// multiplication
result = "Total: {count * 2}"
if result != "Total: 10"
    os.output("  ERROR: expected 'Total: 10', got '{result}'")
    errors = errors + 1

// addition
result = "Sum: {price + 5}"
if result != "Sum: 15.5"
    os.output("  ERROR: expected 'Sum: 15.5', got '{result}'")
    errors = errors + 1

// mixed types (number + decimal)
result = "Combined: {count + price}"
if result != "Combined: 15.5"
    os.output("  ERROR: expected 'Combined: 15.5', got '{result}'")
    errors = errors + 1

// division in expression
result = "Half: {count / 2}"
if result != "Half: 2.5"
    os.output("  ERROR: expected 'Half: 2.5', got '{result}'")
    errors = errors + 1

// subtraction in expression
result = "Remaining: {price - 3}"
if result != "Remaining: 7.5"
    os.output("  ERROR: expected 'Remaining: 7.5', got '{result}'")
    errors = errors + 1

// modulo operator
result = "Modulo: {count % 2}"
if result != "Modulo: 1"
    os.output("  ERROR: expected 'Modulo: 1', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 4: Complex Expressions with Parentheses
// ==========================================

result = "Complex: {(count + 3) * 2}"
if result != "Complex: 16"
    os.output("  ERROR: expected 'Complex: 16', got '{result}'")
    errors = errors + 1

score = 85
bonus = 5
result = "Final: {score + bonus}"
if result != "Final: 90"
    os.output("  ERROR: expected 'Final: 90', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 5: Multiple Expressions in One String
// ==========================================

a = 10
b = 20
c = 30
result = "{a} + {b} = {c}"
if result != "10 + 20 = 30"
    os.output("  ERROR: expected '10 + 20 = 30', got '{result}'")
    errors = errors + 1

result = "Results: {count} * 2 = {count * 2}, {count} + {price} = {count + price}"
if result != "Results: 5 * 2 = 10, 5 + 10.5 = 15.5"
    os.output("  ERROR: expected 'Results: 5 * 2 = 10, 5 + 10.5 = 15.5', got '{result}'")
    errors = errors + 1

// mixed types in one string
product = "Laptop"
price = 999.99
in_stock = true
result = "Product: {product}, Price: ${price}, Available: {in_stock}"
if result != "Product: Laptop, Price: $999.99, Available: True"
    os.output("  ERROR: expected 'Product: Laptop, Price: $999.99, Available: True', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 6: Table Interpolation
// ==========================================

user = ("Alice", age = 30)
result = "Name: {user.1}, Age: {user.age}"
if result != "Name: Alice, Age: 30"
    os.output("  ERROR: expected 'Name: Alice, Age: 30', got '{result}'")
    errors = errors + 1

// nested tables
company = (
    name = "Apex",
    address = (city = "Dubai")
)
result = "Company: {company.name}, City: {company.address.city}"
if result != "Company: Apex, City: Dubai"
    os.output("  ERROR: expected 'Company: Apex, City: Dubai', got '{result}'")
    errors = errors + 1

// ==========================================
// SECTION 7: Edge Cases
// ==========================================

// strings with quotes inside
result = "He said: "Hello!""
if result != "He said: "Hello!""
    os.output("  ERROR: expected 'He said: "Hello!"', got '{result}'")
    errors = errors + 1

// empty string
result = ""
if result != ""
    os.output("  ERROR: expected empty string, got '{result}'")
    errors = errors + 1

// ==========================================
// Final Results
// ==========================================

os.output("Total errors in string_interpolation.apex: {errors}")

if errors == 0
    os.output("All tests in string_interpolation.apex passed!")
else
    os.output("{errors} test(s) failed in string_interpolation.apex.")