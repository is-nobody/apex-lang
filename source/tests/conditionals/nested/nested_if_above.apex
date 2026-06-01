import os

function nested_if_above()
    result = ""
    x = 25
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("result = {result}, expected above")
nested_if_above()
