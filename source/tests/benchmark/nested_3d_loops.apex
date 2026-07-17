import os

function nested_3d_loops()
    start = os.time()
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
    
    elapsed = os.time() - start
    os.output("Nested 3D loops: {sum}, elapsed {elapsed}")

nested_3d_loops()