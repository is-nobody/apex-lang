no_quota = [
    MY_KEY = "value"  // Table keys must be strings,
                      // got identifier 'MY_KEY'. Use "MY_KEY" instead
]

undefined_variable = [
    "MY_KEY" = value  // Undefined variable or function 'value'
]

missed_element = ["a", , "b"]

dot = missed_element.a  // Dot access is restricted to imported modules.
                      // Use bracket notation '[]' for table access.

x = 5
x["field"]  // Cannot access element of non-table type number

expected_rbrace = [
    "MY_KEY" = "value"  // Expected ']' after table literal