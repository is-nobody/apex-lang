// source/tests/errors/tables.apex
// Implementation of Table Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: table keys must be strings, got identifier
no_quota = [
    MY_KEY = "value"  // Table keys must be strings,
                      // got identifier 'MY_KEY'. Use "MY_KEY" instead
]

// error: undefined variable or function 'value'
undefined_variable = [
    "MY_KEY" = value  // Undefined variable or function 'value'
]

// error: missing element between commas
missed_element = ["a", , "b"]

// error: dot access restricted to imported modules
dot = missed_element.a  // Dot access is restricted to imported modules.
                        // Use bracket notation '[]' for table access.

// error: cannot access element of non-table type
x = 5
x["field"]  // Cannot access element of non-table type number

// error: expected ']' after table literal
expected_rbrace = [
    "MY_KEY" = "value"  // Expected ']' after table literal