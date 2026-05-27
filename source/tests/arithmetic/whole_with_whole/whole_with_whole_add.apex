// whole_with_whole_add.apex
import os

function whole_with_whole_add()
    errors = 0
    
    result = 10 + 3
    if result != 13
        os.output("  ERROR: 10 + 3 = {result}, expected 13")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_whole_add.apex.")

whole_with_whole_add()