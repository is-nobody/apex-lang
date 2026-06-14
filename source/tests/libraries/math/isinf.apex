import os
import math

function test_isinf()
    result = math.isinf(math.inf())
    os.output("math.isinf(math.inf()) = {result}, expected true")

    result = math.isinf(1)
    os.output("math.isinf(1) = {result}, expected false")
    
    result = math.isinf(1e308)
    os.output("math.isinf(1e308) = {result}, expected false")

test_isinf()