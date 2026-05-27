import os

function or_false_false()
    errors = 0
    
    result = false or false
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in or_false_false.apex.")

or_false_false()
