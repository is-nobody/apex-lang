import os

function ackermann(m, n)
    if m == 0
        return n + 1
    if n == 0
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

function ackermann36()
    start = os.time()
    result = ackermann(3, 6)
    elapsed = os.time() - start
    os.output("Ackermann(3,6) = {result}, elapsed {elapsed}")

function ackermann38()
    start = os.time()
    result = ackermann(3, 8)
    elapsed = os.time() - start
    os.output("Ackermann(3,8) = {result}, elapsed {elapsed}")

function ackermann310()
    start = os.time()
    result = ackermann(3, 10)
    elapsed = os.time() - start
    os.output("Ackermann(3,10) = {result}, elapsed {elapsed}")

ackermann36()
ackermann38()
ackermann310()