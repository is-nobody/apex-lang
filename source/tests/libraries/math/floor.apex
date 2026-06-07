import os
import math

function floor()
    result = math.floor(3.7)
    os.output("math.floor(3.7) = {result}, expected 3")

    result = math.floor(-3.7)
    os.output("math.floor(-3.7) = {result}, expected -4")

    result = math.floor(3.2)
    os.output("math.floor(3.2) = {result}, expected 3")

floor()