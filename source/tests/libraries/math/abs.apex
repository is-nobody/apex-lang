import os
import math

function abs()
    result = math.abs(-5)
    os.output("math.abs(-5) = {result}, expected 5")
    result = math.abs(3.14)
    os.output("math.abs(3.14) = {result}, expected 3.14")

abs()