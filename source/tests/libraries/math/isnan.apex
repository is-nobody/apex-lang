import os
import math

function test_isnan()
    os.output(math.sqrt(-1))
    result = math.isnan(math.sqrt(-1))
    os.output("math.isnan(sqrt(-1)) = {result}, expected true")

    result = math.isnan(1)
    os.output("math.isnan(1) = {result}, expected false")
    
    os.output(math.log(-1))
    result = math.isnan(math.log(-1))
    os.output("math.isnan(log(-1)) = {result}, expected true")

test_isnan()