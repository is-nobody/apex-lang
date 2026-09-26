// source/tests/benchmark/float_arithmetic.apex
// Implementation of Float Arithmetic Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import datetime

// benchmark: 50M iterations of a tiny dependent arithmetic chain
function float_arithmetic()
    start = datetime.timestamp()
    x = 1.0
    y = 0.0000001
    z = 0.0
    for i = 1, 50000000
        z = z + x * y
        x = x + 0.0000001
        y = y * 1.0000001
    elapsed = datetime.timestamp() - start
    os.output("Float arithmetic: {z}, elapsed {elapsed}")

// execute benchmark
float_arithmetic()