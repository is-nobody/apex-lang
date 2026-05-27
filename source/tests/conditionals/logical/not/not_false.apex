import os

function not_false()
    errors = 0
    
    result = not false
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in not_false.apex.")

not_false()
