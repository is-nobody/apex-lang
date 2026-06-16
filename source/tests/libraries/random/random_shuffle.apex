import random
import os
import table

function test_shuffle_preserves_size()
    t_shuf = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    original_size = table.size(t_shuf)
    random.shuffle(t_shuf)
    if table.size(t_shuf) == original_size
        os.output("PASS: shuffle() preserves size")
    else
        os.output("FAIL: shuffle() changed size")

function test_shuffle_single_element()
    t_single = (val = 1)
    random.shuffle(t_single)
    if table.size(t_single) == 1
        os.output("PASS: shuffle(single) works")
    else
        os.output("FAIL: shuffle(single) failed")

function test_shuffle_empty()
    t_empty = ()
    res = random.shuffle(t_empty)
    if res == false
        os.output("PASS: shuffle(empty) returns false")
    else
        os.output("FAIL: shuffle(empty) should return false")

test_shuffle_preserves_size()
test_shuffle_single_element()
test_shuffle_empty()