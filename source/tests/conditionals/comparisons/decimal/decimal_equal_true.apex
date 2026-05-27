import os

function decimal_equal_true()
    errors = 0
    
    result = 3.5 == 3.5
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_equal_true.apex.")

decimal_equal_true()
