// source/tests/benchmark/table_copy.apex
// Implementation of Table Copy Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import table
import sys

// benchmark: copies a table with 10 million integer entries
function table_copy_10m()
    t = []
    for i = 1, 10000000
        t[i] = i * 2
    
    start = sys.time()
    copy = table.copy(t)
    elapsed = sys.time() - start
    os.output("Copy size: {table.size(copy)}, elapsed {elapsed}")

// execute benchmark
table_copy_10m()