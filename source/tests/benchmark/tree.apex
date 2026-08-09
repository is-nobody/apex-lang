// source/tests/benchmark/tree.apex
// Implementation of Binary Tree Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// recursively builds and sums a binary tree of depth n
// each node returns 1 + sum(left) + sum(right)
function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

// benchmark: tree(25) - evaluates 2^26 - 1 = 67 million recursive calls
function tree25()
    start = sys.time()
    result = tree_sum(25)
    elapsed = sys.time() - start
    os.output("tree(25) = {result}, elapsed {elapsed}")

// execute benchmark
tree25()