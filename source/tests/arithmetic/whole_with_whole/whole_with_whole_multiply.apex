// whole_with_whole_multiply.apex
import os

function whole_with_whole_multiply()
    errors = 0
    
    result = 10 * 3
    if result != 30
        os.output("  ERROR: 10 * 3 = {result}, expected 30")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_whole_multiply.apex.")

whole_with_whole_multiply()