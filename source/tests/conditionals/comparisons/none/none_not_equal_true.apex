import os

function none_not_equal_true()
    errors = 0
    
    result = none != false
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in none_not_equal_true.apex.")

none_not_equal_true()
