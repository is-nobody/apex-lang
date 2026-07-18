import os
import sys

function for_1m()
    start = sys.time()
    x = 0
    for i = 1, 1000000
        x = x + i
    elapsed = sys.time() - start
    os.output("1000000 iterations for: {x}, elapsed {elapsed}")

function for_10m()
    start = sys.time()
    x = 0
    for i = 1, 10000000
        x = x + i
    elapsed = sys.time() - start
    os.output("10000000 iterations for: {x}, elapsed {elapsed}")

function for_100m()
    start = sys.time()
    x = 0
    for i = 1, 100000000
        x = x + i
    elapsed = sys.time() - start
    os.output("100000000 iterations for: {x}, elapsed {elapsed}")

for_1m()
for_10m()
for_100m()