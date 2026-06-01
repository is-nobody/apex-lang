import os

function if_else_true()
    result = ""
    age = 25
    if age >= 18
        result = "adult"
    else
        result = "minor"
    os.output("result = {result}, expected adult")
if_else_true()
