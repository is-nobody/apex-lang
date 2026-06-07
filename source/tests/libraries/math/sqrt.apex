import os
import math

function sqrt()
    result = math.sqrt(16)
    os.output("math.sqrt(16) = {result}, expected 4")

    result = math.sqrt(2)
    os.output("math.sqrt(2) = {result}, expected 1.4142135623731")

    result = math.sqrt(0)
    os.output("math.sqrt(0) = {result}, expected 0")

    result = math.sqrt(-1)
    os.output("math.sqrt(-1) = {result}, expected false")

sqrt()