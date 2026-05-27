import os

function none_not_equal_false()
    errors = 0
    
    result = none != none
    if result != false
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in none_not_equal_false.apex.")

none_not_equal_false()
