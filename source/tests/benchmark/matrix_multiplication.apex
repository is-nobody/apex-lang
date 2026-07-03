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
    
    v1 = c[1][1]
    v25 = c[25][25]
    v50 = c[50][50]
    elapsed = os.time() - start
    os.output("C[1][1]={v1}, expected 343400, C[25][25]={v25}, expected 11615000, C[50][50]={v50}, expected 29542500")
    os.output("Time: {elapsed} seconds")

matrix_multiplication()