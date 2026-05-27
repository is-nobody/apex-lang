// priority_same_precedence_left_to_right.apex
import os

function priority_same_precedence_left_to_right()
    errors = 0
    
    result = 20 - 5 - 3
    if result != 12
        os.output("  ERROR: 20 - 5 - 3 = {result}, expected 12")
        errors = errors + 1
    
    result = 40 / 4 / 2
    if result != 5
        os.output("  ERROR: 40 / 4 / 2 = {result}, expected 5")
        errors = errors + 1
    
    result = 10 - 5 + 2
    if result != 7
        os.output("  ERROR: 10 - 5 + 2 = {result}, expected 7")
        errors = errors + 1
    
    result = 12 * 3 / 2
    if result != 18
        os.output("  ERROR: 12 * 3 / 2 = {result}, expected 18")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in priority_same_precedence_left_to_right.apex.")

priority_same_precedence_left_to_right()