import os

function benchmark()
    start = os.time()
    x = 0
    i = 1
    while i < 100000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("100000000 iterations while: {x}, excepted 5.00000005e+15")
    os.output("Time: {elapsed} seconds")

benchmark()