import os

function benchmark()
    start = os.time()
    x = 0
    for i = 1, 100000000
        x = x + i
    elapsed = os.time() - start
    os.output("100000000 iterations for: {x}, excepted 5.00000005e+15")
    os.output("Time: {elapsed} seconds")

benchmark()