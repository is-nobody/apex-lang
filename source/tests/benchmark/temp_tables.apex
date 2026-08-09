// source/tests/benchmark/temp_tables.apex
// Implementation of Temporary Tables Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: creates 1 million temporary tables (10 entries each) and sums their 5th element
function temp_tables()
    start = sys.time()
    sum = 0
    for i = 1, 1000000
        temp = []
        for j = 1, 10
            temp[j] = i * j
        sum = sum + temp[5]
    elapsed = sys.time() - start
    os.output("Temporary tables: {sum}, elapsed {elapsed}")

// execute benchmark
temp_tables()