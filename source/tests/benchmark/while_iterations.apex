import os

function while_1m()
    start = os.time()
    x = 0
    i = 1
    while i < 1000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("1000000 iterations while: {x}, expected 500000500000")
    os.output("Time: {elapsed} seconds")

function while_10m()
    start = os.time()
    x = 0
    i = 1
    while i < 10000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("10000000 iterations while: {x}, expected 50000005000000")
    os.output("Time: {elapsed} seconds")

function while_100m()
    start = os.time()
    x = 0
    i = 1
    while i < 100000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("100000000 iterations while: {x}, expected 5.00000005e+15")
    os.output("Time: {elapsed} seconds")

while_1m()
while_10m()
while_100m()