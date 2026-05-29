import os

function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

function benchmark()
    os.output("Apex: fib(35) recursive")
    start = os.time()
    result = fib(35)
    elapsed = os.time() - start
    os.output("Result: {result}")
    os.output("Time: {elapsed} seconds")

benchmark()