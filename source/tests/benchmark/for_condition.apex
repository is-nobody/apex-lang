import os

function for_1m()
    start = os.time()
    x = 0
    i = 1
    for i < 1000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("1000000 iterations for: {x}, expected 500000500000")
    os.output("Time: {elapsed} seconds")

function for_10m()
    start = os.time()
    x = 0
    i = 1
    for i < 10000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("10000000 iterations for: {x}, expected 50000005000000")
    os.output("Time: {elapsed} seconds")

function for_100m()
    start = os.time()
    x = 0
    i = 1
    for i < 100000001
        x = x + i
        i = i + 1
    elapsed = os.time() - start
    os.output("100000000 iterations for: {x}, expected 5.00000005e+15")
    os.output("Time: {elapsed} seconds")

for_1m()
for_10m()
for_100m()