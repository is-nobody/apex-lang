// source/tests/benchmark/for_counter.apex
// Implementation of Counter For Loop Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: 100 million iterations using counter-based for loop
function for_counter_100m()
    start = sys.time()
    x = 0
    for i = 1, 100000000
        x = x + i
    elapsed = sys.time() - start
    os.output("100000000 iterations for counter: {x}, elapsed {elapsed}")

// execute benchmark
for_counter_100m()