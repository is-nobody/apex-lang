// whole_with_whole_modulo.apex
import os

function whole_with_whole_modulo()
    errors = 0
    
    result = 10 % 3
    if result != 1
        os.output("  ERROR: 10 % 3 = {result}, expected 1")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in whole_with_whole_modulo.apex.")

whole_with_whole_modulo()