import os

function matrix_multiplication()
    start = os.time()
    n = 100
    a = []
    b = []
    c = []
    
    for i = 1, n
        a[i] = []
        b[i] = []
        c[i] = []
        for j = 1, n
            a[i][j] = i + j
            b[i][j] = i * j
            c[i][j] = 0
    
    for i = 1, n
        for j = 1, n
            sum = 0
            for k = 1, n
                sum = sum + a[i][k] * b[k][j]
            c[i][j] = sum
    
    v50 = c[50][50]
    elapsed = os.time() - start
    os.output("Matrix multiplication: C[50][50]={v50}, elapsed {elapsed}")

matrix_multiplication()