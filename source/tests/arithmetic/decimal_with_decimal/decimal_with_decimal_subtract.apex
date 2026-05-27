// decimal_with_decimal_subtract.apex
import os

function decimal_with_decimal_subtract()
    errors = 0
    
    result = 3.5 - 1.5
    if result != 2.0
        os.output("  ERROR: 3.5 - 1.5 = {result}, expected 2.0")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_with_decimal_subtract.apex.")

decimal_with_decimal_subtract()