// whole_with_whole_subtract.apex
import os

function whole_with_whole_subtract()
    errors = 0
    
    result = 10 - 3
    if result != 7
        os.output("  ERROR: 10 - 3 = {result}, expected 7")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_whole_subtract.apex.")

whole_with_whole_subtract()