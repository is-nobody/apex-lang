import os

function if_elif_else_first()
    errors = 0
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
    if result != "A"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in if_elif_else_first.apex.")
if_elif_else_first()
