import os
import math

function test_trunc()
    result = math.trunc(3.9)
    os.output("math.trunc(3.9) = {result}, expected 3")

    result = math.trunc(-3.9)
    os.output("math.trunc(-3.9) = {result}, expected -3")

test_trunc()