import os

function whole_not_equal_true()
    errors = 0
    
    result = 5 != 3
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_not_equal_true.apex.")

whole_not_equal_true()
