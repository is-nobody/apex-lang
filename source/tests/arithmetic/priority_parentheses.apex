// priority_parentheses.apex
import os

function priority_parentheses()
    result = (2 + 3) * 5
    os.output("(2 + 3) * 5 = {result}, expected 25")
    
    result = 2 * (5 - 3)
    os.output("2 * (5 - 3) = {result}, expected 4")
    
    result = 20 / (3 + 2)
    os.output("20 / (3 + 2) = {result}, expected 4")
    
    result = (8 - 3) * (2 + 1)
    os.output("(8 - 3) * (2 + 1) = {result}, expected 15")

priority_parentheses()