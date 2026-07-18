import os
import sys

function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

function fib30()
    start = sys.time()
    result = fib(30)
    elapsed = sys.time() - start
    os.output("fib(30) recursive = {result}, elapsed {elapsed}")

function fib35()
    start = sys.time()
    result = fib(35)
    elapsed = sys.time() - start
    os.output("fib(35) recursive = {result}, elapsed {elapsed}")

fib30()
fib35()