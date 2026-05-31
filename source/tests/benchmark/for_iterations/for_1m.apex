import os

function benchmark()
    start = os.time()
    x = 0
    for i = 1, 1000000
        x = x + i
    elapsed = os.time() - start
    os.output("1000000 iterations for: {x}, excepted 500000500000")
    os.output("Time: {elapsed} seconds")

benchmark()