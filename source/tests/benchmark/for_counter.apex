import os
import sys

function for_counter_100m()
    start = sys.time()
    x = 0
    for i = 1, 100000000
        x = x + i
    elapsed = sys.time() - start
    os.output("100000000 iterations for counter: {x}, elapsed {elapsed}")

for_counter_100m()