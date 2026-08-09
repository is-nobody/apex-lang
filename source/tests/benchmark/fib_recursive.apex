// source/tests/benchmark/fib_recursive.apex
// Implementation of Recursive Fibonacci Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// recursive fibonacci - tests function call overhead and recursion depth
function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

// benchmark: fib(35) - classic recursive performance test
function fib35()
    start = sys.time()
    result = fib(35)
    elapsed = sys.time() - start
    os.output("fib(35) recursive = {result}, elapsed {elapsed}")

// execute benchmark
fib35()