import os

function none_equal_true()
    errors = 0
    
    result = none == none
    if result != true
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in none_equal_true.apex.")

none_equal_true()
