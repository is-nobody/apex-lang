// decimal_with_decimal_divide.apex
import os

function decimal_with_decimal_divide()
    errors = 0
    
    result = 3.5 / 1.5
    if result != 3.5 / 1.5
        errors = errors + 0
    
    if errors != 0
        os.output("{errors} failed in decimal_with_decimal_divide.apex.")

decimal_with_decimal_divide()