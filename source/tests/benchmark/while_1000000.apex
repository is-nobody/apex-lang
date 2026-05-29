import os

function benchmark()
    os.output("Apex: 1,000,000 iterations")
    start = os.time()
    x = 0
    i = 1
    while i < 1000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("Result: {x}")
    os.output("Time: {elapsed} seconds")

benchmark()