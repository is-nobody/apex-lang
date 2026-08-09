// source/tests/benchmark/table_nested_access.apex
// Implementation of Table Nested Access Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: nested table access with 1000*100*10 = 1 million elements
function table_nested_access()
    start = sys.time()
    root = []
    
    // build 3D nested table structure
    for i = 1, 1000
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 10
                root[i][j][k] = i * j * k
    
    // sum all values via triple nested access
    sum = 0
    for i = 1, 1000
        for j = 1, 100
            for k = 1, 10
                sum = sum + root[i][j][k]
    
    elapsed = sys.time() - start
    os.output("Table nested access: {sum}, elapsed {elapsed}")

// execute benchmark
table_nested_access()