import os

function and_true_false()
    errors = 0
    
    result = true and false
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in and_true_false.apex.")

and_true_false()
