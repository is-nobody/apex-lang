import os

function string_not_equal_false()
    errors = 0
    
    result = "hello" != "hello"
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in string_not_equal_false.apex.")

string_not_equal_false()
