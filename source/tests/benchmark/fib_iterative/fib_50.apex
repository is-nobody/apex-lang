import os

function fib_iter(n)
    if n < 2
        return n
    a = 0
    b = 1
    for i = 2, n
        c = a + b
        a = b
        b = c
    return b

function benchmark()
    n = 50
    iterations = 1000000
    start = os.time()
    result = 0
    for k = 1, iterations
        result = fib_iter(n)
    elapsed = os.time() - start
    os.output("fib_iter(50) x 1000000 = {result}, excepted 12586269025")
    os.output("Time: {elapsed} seconds")

benchmark()