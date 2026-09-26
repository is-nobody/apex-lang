// source/tests/benchmark/matrix_multiplication.apex
// Implementation of Matrix Multiplication Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import datetime

// benchmark: 250x250 matrix multiplication (~15.6 million operations)
function matrix_multiplication()
    start = datetime.timestamp()
    n = 250
    a = []
    b = []
    c = []
    
    // initialize matrices a and b, allocate c
    for i = 1, n
        a[i] = []
        b[i] = []
        c[i] = []
        for j = 1, n
            a[i][j] = i + j
            b[i][j] = i * j
            c[i][j] = 0
    
    // multiply matrices: c = a * b
    for i = 1, n
        for j = 1, n
            sum = 0
            for k = 1, n
                sum = sum + a[i][k] * b[k][j]
            c[i][j] = sum
    
    v50 = c[250][250]
    elapsed = datetime.timestamp() - start
    os.output("Matrix multiplication: C[250][250]={v50}, elapsed {elapsed}")

// execute benchmark
matrix_multiplication()