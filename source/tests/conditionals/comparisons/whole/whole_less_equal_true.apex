import os

function whole_less_equal_true()
    errors = 0
    
    result = 5 <= 10
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_less_equal_true.apex.")

whole_less_equal_true()
