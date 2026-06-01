// priority_multiple_levels.apex
import os

function priority_multiple_levels()
    
    result = 2 + 3 * 4 - 1
    os.output("2 + 3 * 4 - 1 = {result}, expected 13")
    
    result = 5 * 2 + 3 * 5
    os.output("5 * 2 + 3 * 5 = {result}, expected 25")
    
    result = 20 / 4 - 3 * 1 + 2
    os.output("20 / 4 - 3 * 1 + 2 = {result}, expected 4")
    
    result = 10 % 3 + 2 * 4
    os.output("10 % 3 + 2 * 4 = {result}, expected 9")
    

priority_multiple_levels()