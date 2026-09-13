// source/tests/benchmark/table_dynamic_read.apex
// Implementation of Dynamic Integer Index Table Read Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: reads 10M elements via dynamic loop-counter key
function table_dynamic_read()
    n = 10000000
    t = []

    // build phase — uses OP_TABLE_SET (dynamic), interpreted by design
    for i = 1, n
        t[i] = i * 2

    start = sys.time()
    sum = 0
    // read phase — OP_TABLE_GET with key == loop counter → native JIT
    for i = 1, n
        sum = sum + t[i]
    elapsed = sys.time() - start

    os.output("Dynamic read: {sum}, elapsed {elapsed}")

// execute benchmark
table_dynamic_read()