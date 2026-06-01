import os


// number() tests
result = number("42")
os.output("number(42) = {result}, expected 42")

result = number("3.14")
os.output("number(3.14) = {result}, expected 3.14")

result = number(true)
os.output("number(true) = {result}, expected 1")

result = number(false)
os.output("number(false) = {result}, expected 0")

result = number("hello")
os.output("number(hello) = {result}, expected none")

result = number(none)
os.output("number(none) = {result}, expected none")

// string() tests
result = string(42)
os.output("string(42) = {result}, expected 42")

result = string(3.14)
os.output("string(3.14) = {result}, expected 3.14")

result = string(true)
os.output("string(true) = {result}, expected true")

result = string(false)
os.output("string(false) = {result}, expected false")

result = string(none)
os.output("string(none) = {result}, expected none")

// type-checked function parameters
function add(a == number, b == number)
    return a + b

result = add(5, 6)
os.output("add(5, 6) = {result}, expected 11")

function greet(name == string)
    return "Hello, {name}"

result = greet("Friend")
os.output("greet(Friend) = {result}, expected Hello, Friend")

function process_status(active == boolean)
    if active
        return "Status: ACTIVE"
    else
        return "Status: INACTIVE"

result = process_status(true)
os.output("process_status(true) = {result}, expected Status: ACTIVE")

result = process_status(false)
os.output("process_status(false) = {result}, expected Status: INACTIVE")

function check_presence(value)
    if value == none
        return "empty"
    else
        return "has_value"

result = check_presence(none)
os.output("check_presence(none) = {result}, expected empty")

result = check_presence("something")
os.output("check_presence(something) = {result}, expected has_value")

// practical usage
num1 = number("42")
num2 = number("58")
result = num1 + num2
os.output("num1 + num2 = {result}, expected 100")

age = 25
result = string(age)
os.output("string(age) = {result}, expected 25")

// final result
