import random
import os

function test_expovariate_positive()
    exp = random.expovariate(1.0)
    if type(exp) == "number" and exp >= 0
        os.output("PASS: expovariate() positive number")
    else
        os.output("FAIL: expovariate() error")

function test_expovariate_zero_lambda()
    res = random.expovariate(0.0)
    if res == false
        os.output("PASS: expovariate(0) returns false")
    else
        os.output("FAIL: expovariate(0) should return false")

test_expovariate_positive()
test_expovariate_zero_lambda()