import os

function nested_if_between()
    result = ""
    x = 15
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("result = {result}, expected between")
nested_if_between()
