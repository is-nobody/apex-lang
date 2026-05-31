import os

function benchmark()
    start = os.time()
    x = 0
    i = 1
    while i < 1000000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("1000000000 iterations while: {x}, excepted 5.00000000067109e+17")
    os.output("Time: {elapsed} seconds")

benchmark()