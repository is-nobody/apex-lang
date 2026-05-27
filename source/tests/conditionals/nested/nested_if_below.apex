import os

function nested_if_below()
    errors = 0
    result = ""
    x = 5
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    if result != "below"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in nested_if_below.apex.")
nested_if_below()
