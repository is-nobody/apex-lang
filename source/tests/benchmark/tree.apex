import os

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

start = os.time()
result = tree_sum(20)
elapsed = os.time() - start
os.output("Binary tree(20): {elapsed} seconds, result={result}")