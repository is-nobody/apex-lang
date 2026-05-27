import os

function or_true_true()
    errors = 0
    
    result = true or true
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in or_true_true.apex.")

or_true_true()
