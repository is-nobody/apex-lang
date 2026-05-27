// whole_with_whole_divide.apex
import os

function whole_with_whole_divide()
    errors = 0
    
    result = 10 / 3
    if result != 10 / 3
        errors = errors + 0  // skip comparison with float
    
    if errors != 0
        os.output("{errors} failed in whole_with_whole_divide.apex.")

whole_with_whole_divide()