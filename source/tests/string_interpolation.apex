import os

result = ""

// ==========================================
// SECTION 1: Basic Interpolation
// ==========================================

name = "Alice"
age = 30
result = "Hello, {name}!"
os.output("result = {result}, expected Hello, Alice!")

result = "{name} is {age} years old"
os.output("result = {result}, expected Alice is 30 years old")

// none in interpolation
x = none
result = "x is {x}"
os.output("result = {result}, expected x is none")

// booleans in interpolation
is_active = true
result = "Status: {is_active}"
os.output("result = {result}, expected Status: True")

is_active = false
result = "Status: {is_active}"
os.output("result = {result}, expected Status: False")

// numbers in interpolation
score = 95
price = 19.99
result = "Score: {score}, Price: {price}"
os.output("result = {result}, expected Score: 95, Price: 19.99")

// concatenation via interpolation
first = "Hello"
second = "World"
result = "{first}, {second}!"
os.output("result = {result}, expected Hello, World!")

// ==========================================
// SECTION 2: Variable Type Changes in Interpolation
// ==========================================

x_change = 10
result = "x was {x_change}"
os.output("result = {result}, expected x was 10")

x_change = "string"
result = "x became {x_change}"
os.output("result = {result}, expected x became string")

x_change = true
result = "x became {x_change}"
os.output("result = {result}, expected x became True")

x_change = none
result = "x became {x_change}"
os.output("result = {result}, expected x became none")

// ==========================================
// SECTION 3: Math Expressions in Interpolation
// ==========================================

count = 5
price = 10.5

// multiplication
result = "Total: {count * 2}"
os.output("result = {result}, expected Total: 10")

// addition
result = "Sum: {price + 5}"
os.output("result = {result}, expected Sum: 15.5")

// mixed types (number + decimal)
result = "Combined: {count + price}"
os.output("result = {result}, expected Combined: 15.5")

// division in expression
result = "Half: {count / 2}"
os.output("result = {result}, expected Half: 2.5")

// subtraction in expression
result = "Remaining: {price - 3}"
os.output("result = {result}, expected Remaining: 7.5")

// modulo operator
result = "Modulo: {count % 2}"
os.output("result = {result}, expected Modulo: 1")

// ==========================================
// SECTION 4: Complex Expressions with Parentheses
// ==========================================

result = "Complex: {(count + 3) * 2}"
os.output("result = {result}, expected Complex: 16")

score = 85
bonus = 5
result = "Final: {score + bonus}"
os.output("result = {result}, expected Final: 90")

// ==========================================
// SECTION 5: Multiple Expressions in One String
// ==========================================

a = 10
b = 20
c = 30
result = "{a} + {b} = {c}"
os.output("result = {result}, expected 10 + 20 = 30")

result = "Results: {count} * 2 = {count * 2}, {count} + {price} = {count + price}"
os.output("result = {result}, expected Results: 5 * 2 = 10, 5 + 10.5 = 15.5")

// mixed types in one string
product = "Laptop"
price = 999.99
in_stock = true
result = "Product: {product}, Price: ${price}, Available: {in_stock}"
os.output("result = {result}, expected Product: Laptop, Price: $999.99, Available: True")

// ==========================================
// SECTION 6: Table Interpolation
// ==========================================

user = ("Alice", age = 30)
result = "Name: {user.1}, Age: {user.age}"
os.output("result = {result}, expected Name: Alice, Age: 30")

// nested tables
company = (
    name = "Apex",
    address = (city = "Dubai")
)
result = "Company: {company.name}, City: {company.address.city}"
os.output("result = {result}, expected Company: Apex, City: Dubai")

// ==========================================
// SECTION 7: Edge Cases
// ==========================================

// strings with quotes inside
result = "He said: "Hello!""
os.output("He said: 'Hello!' = {result}, expected He said: 'Hello!'")

// empty string
result = ""
os.output("expected empty string, got '{result}'")

// ==========================================
// SECTION 8: Nested Interpolation (Table.field inside braces)
// ==========================================

// Basic nested interpolation
person = (name = "John", field = "name")
result = "Name: {person.{person.field}}"
os.output("result = {result}, expected Name: John")

// Two-level nested interpolation
data = (profile = (user = (username = "alice")), level1 = "profile", level2 = "user")
result = "Username: {data.{data.level1}.{data.level2}.username}"
os.output("result = {result}, expected Username: alice")

// Mixed with math in nested braces
item = (value = 10, multiplier = "value")
result = "Value: {item.{item.multiplier} * 2}"
os.output("result = {result}, expected Value: 20")

// Nested with arithmetic inside inner braces
config = (factor = 3, target = "factor")
result = "Result: {5 + config.{config.target}}"
os.output("result = {result}, expected Result: 8")

// Triple nesting
deep = (a = (b = (c = "hello")), path1 = "a", path2 = "b")
result = "{deep.{deep.path1}.{deep.path2}.c}"
os.output("result = {result}, expected hello")

// Nested with index access
list = (10, 20, 30)
index = 2
result = "Value: {list.{index}}"
os.output("result = {result}, expected Value: 20")

// Complex nested expression
math = (x = 5, y = 3, op = "x", op2 = "y")
result = "Sum: {math.{math.op} + math.{math.op2}}"
os.output("result = {result}, expected Sum: 8")

// ==========================================
// Final Results
// ==========================================

