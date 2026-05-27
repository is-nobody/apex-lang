import os

function string_not_equal_true()
    errors = 0
    
    result = "hello" != "world"
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in string_not_equal_true.apex.")

string_not_equal_true()
