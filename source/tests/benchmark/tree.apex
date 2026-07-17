import os

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

function tree20()
    start = os.time()
    result = tree_sum(20)
    elapsed = os.time() - start
    os.output("tree(20) = {result}, elapsed {elapsed}")

function tree25()
    start = os.time()
    result = tree_sum(25)
    elapsed = os.time() - start
    os.output("tree(25) = {result}, elapsed {elapsed}")

tree20()
tree25()