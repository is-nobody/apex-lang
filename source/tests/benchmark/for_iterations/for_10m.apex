import os

function benchmark()
    start = os.time()
    x = 0
    for i = 1, 10000000
        x = x + i
    elapsed = os.time() - start
    os.output("10000000 iterations for: {x}, excepted 50000005000000")
    os.output("Time: {elapsed} seconds")

benchmark()