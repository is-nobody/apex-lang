import random
import os

function test_triangular_range()
    tri = random.triangular(0, 10, 5)
    if type(tri) == "number" and tri >= 0 and tri <= 10
        os.output("PASS: triangular() range {tri}")
    else
        os.output("FAIL: triangular() range error")

function test_triangular_defaults()
    tri = random.triangular()
    if type(tri) == "number" and tri >= 0 and tri <= 1
        os.output("PASS: triangular() defaults {tri}")
    else
        os.output("FAIL: triangular() defaults error")

test_triangular_range()
test_triangular_defaults()