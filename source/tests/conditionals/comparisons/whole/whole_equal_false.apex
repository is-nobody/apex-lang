import os

function whole_equal_false()
    errors = 0
    
    result = 5 == 3
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_equal_false.apex.")

whole_equal_false()
