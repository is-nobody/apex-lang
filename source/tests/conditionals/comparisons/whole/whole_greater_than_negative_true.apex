import os

function whole_greater_than_negative_true()
    errors = 0
    
    result = -5 > -10
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_greater_than_negative_true.apex.")

whole_greater_than_negative_true()
