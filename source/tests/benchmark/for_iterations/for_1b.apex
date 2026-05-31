import os

function benchmark()
    start = os.time()
    x = 0
    for i = 1, 1000000000
        x = x + i
    elapsed = os.time() - start
    os.output("1000000000 iterations for: {x}, excepted 5.00000000067109e+17,")
    os.output("Time: {elapsed} seconds")

benchmark()