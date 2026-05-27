import os

function if_else_false()
    errors = 0
    result = ""
    age = 16
    if age >= 18
        result = "adult"
    else
        result = "minor"
    if result != "minor"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in if_else_false.apex.")
if_else_false()
