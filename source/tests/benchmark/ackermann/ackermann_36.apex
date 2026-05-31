import os

function ackermann(m, n)
    if m == 0
        return n + 1
    if n == 0
        return ackermann(m - 1, 1)
    return ackermann(m - 1, ackermann(m, n - 1))

start = os.time()
result = ackermann(3, 12)
elapsed = os.time() - start
os.output("Ackermann(3,12) = {result}, excepted 32765")
os.output("Time: {elapsed}")