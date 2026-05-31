import os

function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

function benchmark()
    start = os.time()
    result = fib(45)
    elapsed = os.time() - start
    os.output("fib(45) recursive = {result}, excepted 1134903170")
    os.output("Time: {elapsed} seconds")

benchmark()