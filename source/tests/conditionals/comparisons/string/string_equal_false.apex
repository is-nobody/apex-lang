import os

function string_equal_false()
    errors = 0
    
    result = "hello" == "world"
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in string_equal_false.apex.")

string_equal_false()
