import os

function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

function fib30()
    start = os.time()
    result = fib(30)
    elapsed = os.time() - start
    os.output("fib(30) recursive = {result}, expected 832040")
    os.output("Time: {elapsed} seconds")

function fib35()
    start = os.time()
    result = fib(35)
    elapsed = os.time() - start
    os.output("fib(35) recursive = {result}, expected 9227465")
    os.output("Time: {elapsed} seconds")

fib30()
fib35()