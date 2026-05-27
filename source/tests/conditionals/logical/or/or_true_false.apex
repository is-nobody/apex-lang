import os

function or_true_false()
    errors = 0
    
    result = true or false
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in or_true_false.apex.")

or_true_false()
