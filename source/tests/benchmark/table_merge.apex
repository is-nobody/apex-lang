// source/tests/benchmark/table_merge.apex
// Implementation of Table Merge Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import table
import sys

// benchmark: merges two tables with 5 million entries each (10m total)
function table_merge_10m()
    a = []
    b = []
    for i = 1, 5000000
        a[i] = i
        b[i] = i * 2
    
    start = sys.time()
    merged = table.merge(a, b)
    elapsed = sys.time() - start
    os.output("Merged size: {table.size(merged)}, elapsed {elapsed}")

// execute benchmark
table_merge_10m()