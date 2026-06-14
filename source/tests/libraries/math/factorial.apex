import os
import math

function test_factorial()
    result = math.factorial(5)
    os.output("math.factorial(5) = {result}, expected 120")

    result = math.factorial(0)
    os.output("math.factorial(0) = {result}, expected 1")

test_factorial()