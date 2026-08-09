// source/tests/benchmark/for_condition.apex
// Implementation of Conditional For Loop Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: 100 million iterations using condition-based for loop
function for_condition_100m()
    start = sys.time()
    x = 0
    i = 1
    for i < 100000001
        x = x + i
        i = i + 1
    elapsed = sys.time() - start
    os.output("100000000 iterations for condition: {x}, elapsed {elapsed}")

// execute benchmark
for_condition_100m()