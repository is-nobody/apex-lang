import os

function nested_if_triple()
    result = ""
    x = 15
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("result = {result}, expected nested")
nested_if_triple()
