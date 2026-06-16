import random
import os

function test_gauss_returns_number()
    g = random.gauss(0, 1)
    if type(g) == "number"
        os.output("PASS: gauss() returns number")
    else
        os.output("FAIL: gauss() type error")

test_gauss_returns_number()