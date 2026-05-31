import os

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

function benchmark()
    start = os.time()
    result = tree_sum(30)
    elapsed = os.time() - start
    os.output("tree(30) = {result}, excepted 1073741823")
    os.output("Time: {elapsed} seconds")

benchmark()