// source/tests/benchmark/table_dynamic_read.apex
// Implementation of Dynamic Integer Index Table Read Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import datetime

// benchmark: reads 10M elements via dynamic loop-counter key
function table_dynamic_read()
    n = 10000000
    t = []

    start = datetime.timestamp()

    // build phase
    for i = 1, n
        t[i] = i * 2

    sum = 0

    // read phase
    for i = 1, n
        sum = sum + t[i]
    elapsed = datetime.timestamp() - start

    os.output("Dynamic read: {sum}, elapsed {elapsed}")

// execute benchmark
table_dynamic_read()