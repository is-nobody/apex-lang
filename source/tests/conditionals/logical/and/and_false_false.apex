import os

function and_false_false()
    errors = 0
    
    result = false and false
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in and_false_false.apex.")

and_false_false()
