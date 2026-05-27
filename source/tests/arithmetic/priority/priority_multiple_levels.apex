// priority_multiple_levels.apex
import os

function priority_multiple_levels()
    errors = 0
    
    result = 2 + 3 * 4 - 1
    if result != 13
        os.output("  ERROR: 2 + 3 * 4 - 1 = {result}, expected 13")
        errors = errors + 1
    
    result = 5 * 2 + 3 * 5
    if result != 25
        os.output("  ERROR: 5 * 2 + 3 * 5 = {result}, expected 25")
        errors = errors + 1
    
    result = 20 / 4 - 3 * 1 + 2
    if result != 4
        os.output("  ERROR: 20 / 4 - 3 * 1 + 2 = {result}, expected 4")
        errors = errors + 1
    
    result = 10 % 3 + 2 * 4
    if result != 9
        os.output("  ERROR: 10 % 3 + 2 * 4 = {result}, expected 9")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in priority_multiple_levels.apex.")

priority_multiple_levels()