import os
import math

function test_degrees()
    result = math.degrees(math.pi())
    os.output("math.degrees(pi) = {result}, expected 180")

test_degrees()