// whole_with_decimal_divide.apex
import os

function whole_with_decimal_divide()
    errors = 0
    
    result = 5 / 2.5
    if result != 2.0
        os.output("  ERROR: 5 / 2.5 = {result}, expected 2.0")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_decimal_divide.apex.")

whole_with_decimal_divide()