import random
import os

function test_betavariate_range()
    beta = random.betavariate(2, 5)
    if type(beta) == "number" and beta >= 0 and beta <= 1
        os.output("PASS: betavariate() range [0, 1]")
    else
        os.output("FAIL: betavariate() range error")

function test_betavariate_negative_alpha()
    res = random.betavariate(-1, 2)
    if res == false
        os.output("PASS: betavariate(negative alpha) returns false")
    else
        os.output("FAIL: betavariate(negative alpha) should return false")

function test_betavariate_negative_beta()
    res = random.betavariate(2, -1)
    if res == false
        os.output("PASS: betavariate(negative beta) returns false")
    else
        os.output("FAIL: betavariate(negative beta) should return false")

test_betavariate_range()
test_betavariate_negative_alpha()
test_betavariate_negative_beta()