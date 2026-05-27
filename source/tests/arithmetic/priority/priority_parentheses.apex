// priority_parentheses.apex
import os

function priority_parentheses()
    errors = 0
    
    result = (2 + 3) * 5
    if result != 25
        os.output("  ERROR: (2 + 3) * 5 = {result}, expected 25")
        errors = errors + 1
    
    result = 2 * (5 - 3)
    if result != 4
        os.output("  ERROR: 2 * (5 - 3) = {result}, expected 4")
        errors = errors + 1
    
    result = 20 / (3 + 2)
    if result != 4
        os.output("  ERROR: 20 / (3 + 2) = {result}, expected 4")
        errors = errors + 1
    
    result = (8 - 3) * (2 + 1)
    if result != 15
        os.output("  ERROR: (8 - 3) * (2 + 1) = {result}, expected 15")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in priority_parentheses.apex.")

priority_parentheses()