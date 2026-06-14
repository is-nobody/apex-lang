import os
import math

function test_pow()
    result = math.pow(2, 10)
    os.output("math.pow(2, 10) = {result}, expected 1024")

    result = math.pow(9, 0.5)
    os.output("math.pow(9, 0.5) = {result}, expected 3")

test_pow()