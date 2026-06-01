// priority_mixed_without_parentheses.apex
import os

function priority_mixed_without_parentheses()
    
    result = 4 * 3 + 5
    os.output("4 * 3 + 5 = {result}, expected 17")
    
    result = 4 + 3 * 5
    os.output("4 + 3 * 5 = {result}, expected 19")
    
    result = 30 / 3 + 4
    os.output("30 / 3 + 4 = {result}, expected 14")
    
    result = 30 + 9 / 3
    os.output("30 + 9 / 3 = {result}, expected 33")
    
    result = 18 - 2 * 4
    os.output("18 - 2 * 4 = {result}, expected 10")
    
    result = 2 * 8 - 3
    os.output("2 * 8 - 3 = {result}, expected 13")
    
    result = 24 / 4 - 3
    os.output("24 / 4 - 3 = {result}, expected 3")
    
    result = 24 - 12 / 3
    os.output("24 - 12 / 3 = {result}, expected 20")
    
    result = 13 % 5 + 2
    os.output("13 % 5 + 2 = {result}, expected 5")
    
    result = 13 + 5 % 3
    os.output("13 + 5 % 3 = {result}, expected 15")
    

priority_mixed_without_parentheses()