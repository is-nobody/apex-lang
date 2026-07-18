import os
import sys

function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

function tree20()
    start = sys.time()
    result = tree_sum(20)
    elapsed = sys.time() - start
    os.output("tree(20) = {result}, elapsed {elapsed}")

function tree25()
    start = sys.time()
    result = tree_sum(25)
    elapsed = sys.time() - start
    os.output("tree(25) = {result}, elapsed {elapsed}")

tree20()
tree25()