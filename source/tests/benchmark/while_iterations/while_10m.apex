import os

function benchmark()
    start = os.time()
    x = 0
    i = 1
    while i < 10000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("10000000 iterations while: {x}, excepted 50000005000000")
    os.output("Time: {elapsed} seconds")

benchmark()