import os

function if_elif_else_last()
    result = ""
    score = 75
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("result = {result}, expected C")
if_elif_else_last()
