import os
import math

function round()
    result = math.round(3.14159)
    os.output("math.round(3.14159) = {result}, expected 3")

    result = math.round(3.14159, 2)
    os.output("math.round(3.14159, 2) = {result}, expected 3.14")

    result = math.round(3.14159, 4)
    os.output("math.round(3.14159, 4) = {result}, expected 3.1416")

round()