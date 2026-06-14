import os
import math

function test_gcd()
    result = math.gcd(12, 8)
    os.output("math.gcd(12, 8) = {result}, expected 4")

    result = math.gcd(54, 24)
    os.output("math.gcd(54, 24) = {result}, expected 6")

test_gcd()