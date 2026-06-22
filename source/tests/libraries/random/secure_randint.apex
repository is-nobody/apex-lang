import random
import os

n = 100
res = random.secure_randint(n)
if res >= 0 and res < n
    os.output("PASS: secure_randint(100) returns value in [0, 99]")
else
    os.output("FAIL: secure_randint(100) out of range")