import os

function if_elif_else_middle()
    result = ""
    score = 85
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("result = {result}, expected B")
if_elif_else_middle()
