import random
import os

function test_seed_reproducibility()
    random.seed(12345)
    r1 = random.random()
    random.seed(12345)
    r2 = random.random()
    if r1 == r2
        os.output("PASS: seed() reproducibility")
    else
        os.output("FAIL: seed() reproducibility")

function test_seed_no_args()
    random.seed()
    r = random.random()
    if type(r) == "number"
        os.output("PASS: seed() no args works")
    else
        os.output("FAIL: seed() no args failed")

test_seed_reproducibility()
test_seed_no_args()