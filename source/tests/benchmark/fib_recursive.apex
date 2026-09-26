// source/tests/benchmark/fib_recursive.apex
// Implementation of Recursive Fibonacci Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import datetime

// recursive fibonacci - tests function call overhead and recursion depth
function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

// benchmark: fib(35) - classic recursive performance test
function fib35()
    start = datetime.timestamp()
    result = fib(35)
    elapsed = datetime.timestamp() - start
    os.output("fib(35) recursive = {result}, elapsed {elapsed}")

// execute benchmark
fib35()