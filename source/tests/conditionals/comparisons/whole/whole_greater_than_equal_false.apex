import os

function whole_greater_than_equal_false()
    errors = 0
    
    result = 5 > 5
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_greater_than_equal_false.apex.")

whole_greater_than_equal_false()
