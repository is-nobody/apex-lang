import os
import math

function asin()
    result = math.asin(0.5)
    os.output("math.asin(0.5) = {result}, expected 0.523598775598299")

    result = math.asin(2)
    os.output("math.asin(2) = {result}, expected false")

asin()