import os
import sys

function table_nested_access()
    start = sys.time()
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
    
    elapsed = sys.time() - start
    os.output("Table nested access: {sum}, elapsed {elapsed}")

table_nested_access()