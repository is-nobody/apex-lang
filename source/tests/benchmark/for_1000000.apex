import os

function benchmark()
    os.output("Apex: 1,000,000 iterations")
    start = os.time()
    x = 0
    for i in range(1, 1000001)
        x = x + i
    elapsed = os.time() - start
    os.output("Result: {x}")
    os.output("Time: {elapsed} seconds")

benchmark()