import os

function not_false()
    result = not false
    os.output("not false = {result}, expected true")

function not_true()
    result = not true
    os.output("not true = {result}, expected false")

not_false()
not_true()