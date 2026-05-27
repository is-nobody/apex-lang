import os

function boolean_equal_true()
    errors = 0
    
    result = true == true
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in boolean_equal_true.apex.")

boolean_equal_true()
