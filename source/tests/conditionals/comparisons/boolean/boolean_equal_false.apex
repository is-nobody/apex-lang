import os

function boolean_equal_false()
    errors = 0
    
    result = true == false
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in boolean_equal_false.apex.")

boolean_equal_false()
