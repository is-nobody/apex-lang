import os

function if_elif_else_else()
    result = ""
    score = 65
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("result = {result}, expected F")

if_elif_else_else()