// decimal_with_decimal_modulo.apex
import os

function decimal_with_decimal_modulo()
    errors = 0
    
    result = 3.5 % 1.5
    if result != 0.5
        os.output("  ERROR: 3.5 % 1.5 = {result}, expected 0.5")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in decimal_with_decimal_modulo.apex.")

decimal_with_decimal_modulo()