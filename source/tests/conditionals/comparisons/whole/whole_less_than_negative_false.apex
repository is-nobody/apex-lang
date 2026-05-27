import os

function whole_less_than_negative_false()
    errors = 0
    
    result = -5 < -10
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_less_than_negative_false.apex.")

whole_less_than_negative_false()
