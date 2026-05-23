import os

// test number
function add(a == number, b == number)
    os.output(a + b)
add(5, 6)                 // 11

// test string
function greet(name == string)
    os.output("Hello, {name}")
greet("Friend")           // Hello, Friend

// test boolean
function process_status(active == boolean)
    if active
        os.output("Status: ACTIVE")
    else
        os.output("Status: INACTIVE")
process_status(true)     // Status: INACTIVE
process_status(false)    // Status: INACTIVE

// test none
function check_presence(value == none)
    if value == none
        os.output("Value is NONE — nothing present")
        return "empty"
    else
        os.output("Value has content")
        return "has_value"
empty_result = check_presence(none)   // Should print: Value is NONE — nothing present

// test number with error
function error_number(first == number, second == number)
    os.output(first + second)
error_number("Hi", 6)            // expected number

// test string with error
function error_string(hello == string)
    os.output(hello)
error_string(5)                  // expected string