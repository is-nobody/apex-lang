// source/tests/benchmark/nested_3d_loops.apex
// Implementation of Nested 3D Loops Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: builds and sums a 100x100x100 3D table (1 million elements)
function nested_3d_loops()
    start = sys.time()
    root = []
    
    // allocate and populate 3D table
    for i = 1, 100
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 100
                root[i][j][k] = i * j * k
    
    // sum all elements via nested iteration
    sum = 0
    for i = 1, 100
        for j = 1, 100
            for k = 1, 100
                sum = sum + root[i][j][k]
    
    elapsed = sys.time() - start
    os.output("Nested 3D loops: {sum}, elapsed {elapsed}")

// execute benchmark
nested_3d_loops()