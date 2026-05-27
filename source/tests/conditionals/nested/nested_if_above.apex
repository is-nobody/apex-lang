import os

function nested_if_above()
    errors = 0
    result = ""
    x = 25
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    if result != "above"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in nested_if_above.apex.")
nested_if_above()
