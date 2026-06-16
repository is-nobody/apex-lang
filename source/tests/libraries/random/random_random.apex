import random
import os

function test_random_range()
    val = random.random()
    if type(val) == "number" and val >= 0.0 and val < 1.0
        os.output("PASS: random() range is {val}")
    else
        os.output("FAIL: random() range or type")

test_random_range()