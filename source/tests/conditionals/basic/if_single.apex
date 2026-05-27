import os

function if_single()
    errors = 0
    result = ""
    x = 10
    if x > 5
        result = "big"
    if result != "big"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in if_single.apex.")
if_single()
