import os
import sys

function for_100m()
    start = sys.time()
    x = 0
    i = 1
    for i < 100000001
        x = x + i
        i = i + 1
    elapsed = sys.time() - start
    os.output("100000000 iterations for: {x}, elapsed {elapsed}")

for_100m()