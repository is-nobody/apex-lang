// source/tests/benchmark/table_int_index.apex
// Implementation of Integer Index Table Access Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: tests direct array part access with integer index [1]
function table_int_index()
    start = sys.time()
    t = [10, 20, 30, 40, 50]
    sum = 0
    
    // read from index 10 millions times
    for i = 1, 10000000
        sum = sum + t[1]
    
    // write to index 10 millions times
    for i = 1, 10000000
        t[1] = i
    
    elapsed = sys.time() - start
    os.output("Table int index: {sum}, elapsed {elapsed}")

// execute benchmark
table_int_index()