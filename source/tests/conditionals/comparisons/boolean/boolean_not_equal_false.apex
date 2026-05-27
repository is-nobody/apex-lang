import os

function boolean_not_equal_false()
    errors = 0
    
    result = true != true
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in boolean_not_equal_false.apex.")

boolean_not_equal_false()
