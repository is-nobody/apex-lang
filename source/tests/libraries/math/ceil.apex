import os
import math

function ceil()
    result = math.ceil(3.7)
    os.output("math.ceil(3.7) = {result}, expected 4")

    result = math.ceil(-3.7)
    os.output("math.ceil(-3.7) = {result}, expected -3")

    result = math.ceil(3.2)
    os.output("math.ceil(3.2) = {result}, expected 4")

ceil()