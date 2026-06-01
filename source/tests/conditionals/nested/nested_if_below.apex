import os

function nested_if_below()
    result = ""
    x = 5
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("result = {result}, expected below")
nested_if_below()
