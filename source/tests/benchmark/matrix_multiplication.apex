// source/tests/benchmark/matrix_multiplication.apex
// Implementation of Matrix Multiplication Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// benchmark: 100x100 matrix multiplication (1 million operations)
function matrix_multiplication()
    start = sys.time()
    n = 100
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
    
    v50 = c[50][50]
    elapsed = sys.time() - start
    os.output("Matrix multiplication: C[50][50]={v50}, elapsed {elapsed}")

// execute benchmark
matrix_multiplication()