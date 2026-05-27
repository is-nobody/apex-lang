import os

function or_false_true()
    errors = 0
    
    result = false or true
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in or_false_true.apex.")

or_false_true()
