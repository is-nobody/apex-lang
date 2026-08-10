// source/tests/benchmark/ackermann.apex
// Implementation of Ackermann Function Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// recursive ackermann function - tests deep recursion and function call overhead
function ackermann(m, n)
    if m == 0
        return n + 1
    if n == 0
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

// benchmark: ackermann(3, 6) - computationally intensive recursion
function ackermann36()
    start = sys.time()
    result = ackermann(3, 6)
    elapsed = sys.time() - start
    os.output("Ackermann(3,6) = {result}, elapsed {elapsed}")

// execute benchmark
ackermann36()