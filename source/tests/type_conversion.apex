import os

errors = 0

// number() tests
result = number("42")
expected = 42
if result != expected
    errors = errors + 1
    os.output("  ERROR: number('42') expected {expected}, got {result}")

result = number("3.14")
expected = 3.14
if result != expected
    errors = errors + 1
    os.output("  ERROR: number('3.14') expected {expected}, got {result}")

result = number(true)
expected = 1
if result != expected
    errors = errors + 1
    os.output("  ERROR: number(true) expected {expected}, got {result}")

result = number(false)
expected = 0
if result != expected
    errors = errors + 1
    os.output("  ERROR: number(false) expected {expected}, got {result}")

result = number("hello")
expected = none
if result != expected
    errors = errors + 1
    os.output("  ERROR: number('hello') expected {expected}, got {result}")

result = number(none)
expected = none
if result != expected
    errors = errors + 1
    os.output("  ERROR: number(none) expected {expected}, got {result}")

// string() tests
result = string(42)
expected = "42"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(42) expected '{expected}', got '{result}'")

result = string(3.14)
expected = "3.14"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(3.14) expected '{expected}', got '{result}'")

result = string(true)
expected = "true"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(true) expected '{expected}', got '{result}'")

result = string(false)
expected = "false"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(false) expected '{expected}', got '{result}'")

result = string(none)
expected = "none"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(none) expected '{expected}', got '{result}'")

// type-checked function parameters
function add(a == number, b == number)
    return a + b

result = add(5, 6)
expected = 11
if result != expected
    errors = errors + 1
    os.output("  ERROR: add(5, 6) expected {expected}, got {result}")

function greet(name == string)
    return "Hello, {name}"

result = greet("Friend")
expected = "Hello, Friend"
if result != expected
    errors = errors + 1
    os.output("  ERROR: greet('Friend') expected '{expected}', got '{result}'")

function process_status(active == boolean)
    if active
        return "Status: ACTIVE"
    else
        return "Status: INACTIVE"

result = process_status(true)
expected = "Status: ACTIVE"
if result != expected
    errors = errors + 1
    os.output("  ERROR: process_status(true) expected '{expected}', got '{result}'")

result = process_status(false)
expected = "Status: INACTIVE"
if result != expected
    errors = errors + 1
    os.output("  ERROR: process_status(false) expected '{expected}', got '{result}'")

function check_presence(value)
    if value == none
        return "empty"
    else
        return "has_value"

result = check_presence(none)
expected = "empty"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_presence(none) expected '{expected}', got '{result}'")

result = check_presence("something")
expected = "has_value"
if result != expected
    errors = errors + 1
    os.output("  ERROR: check_presence('something') expected '{expected}', got '{result}'")

// practical usage
num1 = number("42")
num2 = number("58")
result = num1 + num2
expected = 100
if result != expected
    errors = errors + 1
    os.output("  ERROR: 42 + 58 expected {expected}, got {result}")

age = 25
result = string(age)
expected = "25"
if result != expected
    errors = errors + 1
    os.output("  ERROR: string(25) expected '{expected}', got '{result}'")

// final result
os.output("Total errors in type_conversion.apex: {errors}")

if errors == 0
    os.output("Tests in type_conversion.apex passed!")
else
    os.output("{errors} test(s) failed in type_conversion.apex.")