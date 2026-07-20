import os
import sys

function for_condition_100m()
    start = sys.time()
    x = 0
    i = 1
    for i < 100000001
        x = x + i
        i = i + 1
    elapsed = sys.time() - start
    os.output("100000000 iterations for condition: {x}, elapsed {elapsed}")

for_condition_100m()