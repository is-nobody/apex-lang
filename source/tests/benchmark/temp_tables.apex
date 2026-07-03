import os

function temp_tables()
    start = os.time()
    sum = 0
    for i = 1, 1000000
        temp = []
        for j = 1, 10
            temp[j] = i * j
        sum = sum + temp[5]
    elapsed = os.time() - start
    os.output("Sum: {sum}, expected 2500002500000")
    os.output("Time: {elapsed} seconds")

temp_tables()