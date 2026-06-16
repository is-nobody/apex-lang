import random
import os
import table

function test_sample_valid()
    t_samp = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    s = random.sample(t_samp, 3)
    if type(s) == "table" and table.size(s) == 3
        os.output("PASS: sample() size correct")
    else
        os.output("FAIL: sample() size incorrect")

function test_sample_k_zero()
    t_samp = (1, 2, 3)
    s = random.sample(t_samp, 0)
    if type(s) == "table" and table.size(s) == 0
        os.output("PASS: sample(k=0) empty table")
    else
        os.output("FAIL: sample(k=0) failed")

function test_sample_k_too_large()
    t_samp = (1, 2, 3)
    res = random.sample(t_samp, 20)
    if res == false
        os.output("PASS: sample(k > size) returns false")
    else
        os.output("FAIL: sample(k > size) should return false")

test_sample_valid()
test_sample_k_zero()
test_sample_k_too_large()