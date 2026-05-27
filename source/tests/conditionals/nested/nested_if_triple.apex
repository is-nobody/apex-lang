import os

function nested_if_triple()
    errors = 0
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
    if result != "nested"
        errors = errors + 1
    if errors != 0
        os.output("{errors} failed in nested_if_triple.apex.")
nested_if_triple()
