import os

function table_nested_access()
    start = os.time()
    root = []
    for i = 1, 1000
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 10
                root[i][j][k] = i * j * k
    
    sum = 0
    for i = 1, 1000
        for j = 1, 100
            for k = 1, 10
                sum = sum + root[i][j][k]
    
    elapsed = os.time() - start
    os.output("Sum: {sum}, expected 139013875000")
    os.output("Time: {elapsed} seconds")

table_nested_access()