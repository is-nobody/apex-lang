// decimal_with_decimal_modulo.apex
import os

function decimal_with_decimal_modulo()
    
    result = 3.5 % 1.5
    os.output("3.5 % 1.5 = {result}, expected 0.5")
    

decimal_with_decimal_modulo()