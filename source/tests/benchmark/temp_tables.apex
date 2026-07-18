import os
import sys

function temp_tables()
    start = sys.time()
    sum = 0
    for i = 1, 1000000
        temp = []
        for j = 1, 10
            temp[j] = i * j
        sum = sum + temp[5]
    elapsed = sys.time() - start
    os.output("Temporary tables: {sum}, elapsed {elapsed}")

temp_tables()