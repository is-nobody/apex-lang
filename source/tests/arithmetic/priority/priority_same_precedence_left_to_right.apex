// priority_same_precedence_left_to_right.apex
import os

function priority_same_precedence_left_to_right()
    
    result = 20 - 5 - 3
    os.output("20 - 5 - 3 = {result}, expected 12")
    
    result = 40 / 4 / 2
    os.output("40 / 4 / 2 = {result}, expected 5")
    
    result = 10 - 5 + 2
    os.output("10 - 5 + 2 = {result}, expected 7")
    
    result = 12 * 3 / 2
    os.output("12 * 3 / 2 = {result}, expected 18")
    

priority_same_precedence_left_to_right()