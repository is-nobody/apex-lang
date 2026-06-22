import random
import os

function test_randint_valid()
    random.seed(100)
    i1 = random.randint(1, 10)
    if type(i1) == "number" and i1 >= 1 and i1 <= 10
        os.output("PASS: randint(1, 10) valid")
    else
        os.output("FAIL: randint(1, 10) invalid")

function test_randint_same_bounds()
    i2 = random.randint(5, 5)
    if i2 == 5
        os.output("PASS: randint(5, 5) == 5")
    else
        os.output("FAIL: randint(5, 5)")

function test_randint_reverse_bounds()
    i3 = random.randint(10, 1)
    if type(i3) == "number" and i3 >= 1 and i3 <= 10
        os.output("PASS: randint(10, 1) handled")
    else
        os.output("FAIL: randint(10, 1)")

test_randint_valid()
test_randint_same_bounds()
test_randint_reverse_bounds()