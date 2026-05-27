import os

function whole_greater_equal_equal_true()
    errors = 0
    
    result = 5 >= 5
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_greater_equal_equal_true.apex.")

whole_greater_equal_equal_true()
