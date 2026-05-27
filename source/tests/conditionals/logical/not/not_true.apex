import os

function not_true()
    errors = 0
    
    result = not true
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in not_true.apex.")

not_true()
