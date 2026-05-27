import os

function and_true_true()
    errors = 0
    
    result = true and true
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in and_true_true.apex.")

and_true_true()
