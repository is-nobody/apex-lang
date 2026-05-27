import os

function whole_less_equal_false()
    errors = 0
    
    result = 10 <= 5
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_less_equal_false.apex.")

whole_less_equal_false()
