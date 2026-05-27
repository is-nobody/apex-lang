import os

function if_else_true()
    errors = 0
    result = ""
    age = 25
    if age >= 18
        result = "adult"
    else
        result = "minor"
    if result != "adult"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in if_else_true.apex.")
if_else_true()
