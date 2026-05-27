// priority_mixed_without_parentheses.apex
import os

function priority_mixed_without_parentheses()
    errors = 0
    
    result = 4 * 3 + 5
    if result != 17
        os.output("  ERROR: 4 * 3 + 5 = {result}, expected 17")
        errors = errors + 1
    
    result = 4 + 3 * 5
    if result != 19
        os.output("  ERROR: 4 + 3 * 5 = {result}, expected 19")
        errors = errors + 1
    
    result = 30 / 3 + 4
    if result != 14
        os.output("  ERROR: 30 / 3 + 4 = {result}, expected 14")
        errors = errors + 1
    
    result = 30 + 9 / 3
    if result != 33
        os.output("  ERROR: 30 + 9 / 3 = {result}, expected 33")
        errors = errors + 1
    
    result = 18 - 2 * 4
    if result != 10
        os.output("  ERROR: 18 - 2 * 4 = {result}, expected 10")
        errors = errors + 1
    
    result = 2 * 8 - 3
    if result != 13
        os.output("  ERROR: 2 * 8 - 3 = {result}, expected 13")
        errors = errors + 1
    
    result = 24 / 4 - 3
    if result != 3
        os.output("  ERROR: 24 / 4 - 3 = {result}, expected 3")
        errors = errors + 1
    
    result = 24 - 12 / 3
    if result != 20
        os.output("  ERROR: 24 - 12 / 3 = {result}, expected 20")
        errors = errors + 1
    
    result = 13 % 5 + 2
    if result != 5
        os.output("  ERROR: 13 % 5 + 2 = {result}, expected 5")
        errors = errors + 1
    
    result = 13 + 5 % 3
    if result != 15
        os.output("  ERROR: 13 + 5 % 3 = {result}, expected 15")
        errors = errors + 1
    
    if errors != 0
        os.output("{errors} failed in priority_mixed_without_parentheses.apex.")

priority_mixed_without_parentheses()