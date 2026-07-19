import os
import sys

function ackermann(m, n)
    if m == 0
        return n + 1
    if n == 0
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

function ackermann310()
    start = sys.time()
    result = ackermann(3, 10)
    elapsed = sys.time() - start
    os.output("Ackermann(3,10) = {result}, elapsed {elapsed}")

ackermann310()