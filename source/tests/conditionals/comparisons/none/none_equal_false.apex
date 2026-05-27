import os

function none_equal_false()
    errors = 0
    
    result = none == false
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in none_equal_false.apex.")

none_equal_false()
