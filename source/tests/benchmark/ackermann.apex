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
    os.output("Ackermann(3,6) = {result}, excepted 509")
    os.output("Time: {elapsed}")

function ackermann38()
    start = os.time()
    result = ackermann(3, 8)
    elapsed = os.time() - start
    os.output("Ackermann(3,8) = {result}, excepted 2045")
    os.output("Time: {elapsed}")

function ackermann310()
    start = os.time()
    result = ackermann(3, 10)
    elapsed = os.time() - start
    os.output("Ackermann(3,10) = {result}, excepted 8189")
    os.output("Time: {elapsed}")

function ackermann312()
    start = os.time()
    result = ackermann(3, 12)
    elapsed = os.time() - start
    os.output("Ackermann(3,12) = {result}, excepted 131069")
    os.output("Time: {elapsed}")

function ackermann314()
    start = os.time()
    result = ackermann(3, 14)
    elapsed = os.time() - start
    os.output("Ackermann(3,14) = {result}, excepted 131069")
    os.output("Time: {elapsed}")

function ackermann41()
    start = os.time()
    result = ackermann(4, 1)
    elapsed = os.time() - start
    os.output("Ackermann(4,1) = {result}, excepted 65533")
    os.output("Time: {elapsed}")

ackermann36()
ackermann38()
ackermann310()
ackermann312()
ackermann314()
ackermann41()