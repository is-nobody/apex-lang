// whole_with_decimal_multiply.apex
import os

function whole_with_decimal_multiply()
    errors = 0
    
    result = 5 * 2.5
    if result != 12.5
        os.output("  ERROR: 5 * 2.5 = {result}, expected 12.5")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_decimal_multiply.apex.")

whole_with_decimal_multiply()