import os

function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

function benchmark()
    start = os.time()
    result = fib(40)
    elapsed = os.time() - start
    os.output("fib(40) recursive = {result}, excepted 102334155")
    os.output("Time: {elapsed} seconds")

benchmark()