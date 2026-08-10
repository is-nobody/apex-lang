// source/tests/integration/lib/fib.apex
// Implementation of Recursive Fibonacci for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// recursive fibonacci - tests function call overhead and recursion depth
function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

// benchmark: fib(15) - classic recursive performance test
function fib15()
    result = fib(15)
    return result