// whole_with_decimal_modulo.apex
import os

function whole_with_decimal_modulo()
    errors = 0
    
    result = 5 % 2.5
    if result != 0
        os.output("  ERROR: 5 % 2.5 = {result}, expected 0")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_decimal_modulo.apex.")

whole_with_decimal_modulo()