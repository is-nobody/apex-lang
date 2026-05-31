import os

function benchmark()
    start = os.time()
    x = 0
    i = 1
    while i < 1000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("1000000 iterations while: {x}, excepted 500000500000")
    os.output("Time: {elapsed} seconds")

benchmark()