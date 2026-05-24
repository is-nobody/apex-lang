import os

errors = 0

// ========================================
// number() tests
// ========================================
if number("42") != 42
    errors = errors + 1
    os.output("FAIL: number('42') should be 42")

if number("3.14") != 3.14
    errors = errors + 1
    os.output("FAIL: number('3.14') should be 3.14")

if number(true) != 1
    errors = errors + 1
    os.output("FAIL: number(true) should be 1")

if number(false) != 0
    errors = errors + 1
    os.output("FAIL: number(false) should be 0")

if number("hello") != none
    errors = errors + 1
    os.output("FAIL: number('hello') should be none")

if number(none) != none
    errors = errors + 1
    os.output("FAIL: number(none) should be none")

// ========================================
// string() tests
// ========================================
if string(42) != "42"
    errors = errors + 1
    os.output("FAIL: string(42) should be '42'")

if string(3.14) != "3.14"
    errors = errors + 1
    os.output("FAIL: string(3.14) should be '3.14'")

if string(true) != "true"
    errors = errors + 1
    os.output("FAIL: string(true) should be 'true'")

if string(false) != "false"
    errors = errors + 1
    os.output("FAIL: string(false) should be 'false'")

if string(none) != "none"
    errors = errors + 1
    os.output("FAIL: string(none) should be 'none'")

// ========================================
// Type-checked function parameters
// ========================================
function add(a == number, b == number)
    return a + b

result = add(5, 6)
if result != 11
    errors = errors + 1
    os.output("FAIL: add(5, 6) should be 11")

function greet(name == string)
    return "Hello, {name}"

message = greet("Friend")
if message != "Hello, Friend"
    errors = errors + 1
    os.output("FAIL: greet('Friend') failed")

function process_status(active == boolean)
    if active
        return "Status: ACTIVE"
    else
        return "Status: INACTIVE"

if process_status(true) != "Status: ACTIVE"
    errors = errors + 1
    os.output("FAIL: process_status(true)")

if process_status(false) != "Status: INACTIVE"
    errors = errors + 1
    os.output("FAIL: process_status(false)")

function check_presence(value)
    if value == none
        return "empty"
    else
        return "has_value"

if check_presence(none) != "empty"
    errors = errors + 1
    os.output("FAIL: check_presence(none) should be 'empty'")

if check_presence("something") != "has_value"
    errors = errors + 1
    os.output("FAIL: check_presence('something') should be 'has_value'")

// ========================================
// Practical usage
// ========================================
num1 = number("42")
num2 = number("58")
sum = num1 + num2
if sum != 100
    errors = errors + 1
    os.output("FAIL: 42 + 58 should be 100")

age = 25
text = string(age)
if text != "25"
    errors = errors + 1
    os.output("FAIL: string(25) should be '25'")

// ========================================
// Result
// ========================================
if errors == 0
    os.output("All type conversion tests passed!")
else
    os.output("{errors} test(s) failed")