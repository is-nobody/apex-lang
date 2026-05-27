import os

function boolean_not_equal_true()
    errors = 0
    
    result = true != false
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in boolean_not_equal_true.apex.")

boolean_not_equal_true()
