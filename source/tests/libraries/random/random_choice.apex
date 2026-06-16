import random
import os

function test_choice_valid()
    t = (10, 20, 30, 40, 50)
    c = random.choice(t)
    found = false
    if c == 10 or c == 20 or c == 30 or c == 40 or c == 50
        found = true
    if found
        os.output("PASS: choice() returns element from table")
    else
        os.output("FAIL: choice() invalid result")

function test_choice_empty()
    empty_t = ()
    res = random.choice(empty_t)
    if res == false
        os.output("PASS: choice(empty) returns false")
    else
        os.output("FAIL: choice(empty) should return false")

test_choice_valid()
test_choice_empty()