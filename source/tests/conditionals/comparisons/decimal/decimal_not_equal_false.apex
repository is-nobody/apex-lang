import os

function decimal_not_equal_false()
    errors = 0
    
    result = 3.5 != 3.5
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_not_equal_false.apex.")

decimal_not_equal_false()
