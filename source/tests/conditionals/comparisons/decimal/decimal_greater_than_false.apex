import os

function decimal_greater_than_false()
    errors = 0
    
    result = 3.5 > 5.5
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_greater_than_false.apex.")

decimal_greater_than_false()
