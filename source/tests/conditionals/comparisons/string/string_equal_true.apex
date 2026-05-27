import os

function string_equal_true()
    errors = 0
    
    result = "hello" == "hello"
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in string_equal_true.apex.")

string_equal_true()
