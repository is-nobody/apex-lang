import os

function and_false_true()
    errors = 0
    
    result = false and true
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in and_false_true.apex.")

and_false_true()
