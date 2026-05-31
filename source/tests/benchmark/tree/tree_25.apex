import os

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

function benchmark()
    start = os.time()
    result = tree_sum(25)
    elapsed = os.time() - start
    os.output("tree(25) = {result}, excepted 33554431")
    os.output("Time: {elapsed} seconds")

benchmark()