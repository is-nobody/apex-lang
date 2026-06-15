import os
import math

function log()
    result = math.log(2.71828)
    os.output("math.log(2.71828) = {result}, expected 0.999999327347282")

    result = math.log(100, 10)
    os.output("math.log(100, 10) = {result}, expected 2")

    result = math.log(0)
    os.output("math.log(0) = {result}, expected -inf")

    result = math.log(10, 1)
    os.output("math.log(10, 1) = {result}, expected nan")

log()