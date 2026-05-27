// decimal_with_decimal_multiply.apex
import os

function decimal_with_decimal_multiply()
    errors = 0
    
    result = 3.5 * 1.5
    if result != 5.25
        os.output("  ERROR: 3.5 * 1.5 = {result}, expected 5.25")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_with_decimal_multiply.apex.")

decimal_with_decimal_multiply()