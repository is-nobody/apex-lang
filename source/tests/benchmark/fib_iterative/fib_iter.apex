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

function test_fib(n, expected)
    start = os.time()
    result = fib_iter(n)
    elapsed = os.time() - start
    os.output("fib_iter({n}) = {result}, excepted {expected}")
    os.output("Time: {elapsed} seconds")

function benchmark()
    test_fib(10, 55)
    test_fib(20, 6765)
    test_fib(30, 832040)
    test_fib(50, 12586269025)
    test_fib(90, 2880067194370816120)
    test_fib(100, 354224848179261915075)
    test_fib(150, 9969216677189303386214405760200)
    test_fib(200, 280571172992510140037611932413038677189525)

benchmark()