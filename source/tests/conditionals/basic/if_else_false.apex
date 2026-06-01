import os

function if_else_false()
    result = ""
    age = 16
    if age >= 18
        result = "adult"
    else
        result = "minor"
    os.output("result = {result}, expected minor")
if_else_false()
