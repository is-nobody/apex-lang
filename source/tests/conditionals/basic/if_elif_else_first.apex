import os

function if_elif_else_first()
    result = ""
    score = 95
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("result = {result}, expected A")
if_elif_else_first()
