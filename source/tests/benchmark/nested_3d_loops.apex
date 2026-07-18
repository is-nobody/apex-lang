import os
import sys

function nested_3d_loops()
    start = sys.time()
    root = []
    for i = 1, 100
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 100
                root[i][j][k] = i * j * k
    
    sum = 0
    for i = 1, 100
        for j = 1, 100
            for k = 1, 100
                sum = sum + root[i][j][k]
    
    elapsed = sys.time() - start
    os.output("Nested 3D loops: {sum}, elapsed {elapsed}")

nested_3d_loops()