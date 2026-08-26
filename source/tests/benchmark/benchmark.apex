// source/tests/benchmark/benchmarks.apex
// Combined Benchmark Suite for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string
import sys
import table

// recursive fibonacci - tests function call overhead and recursion depth
function fib(n)
    if n < 2
        return n
    return fib(n - 1) + fib(n - 2)

// benchmark: fib(35) - classic recursive performance test
function fib35()
    start = sys.time()
    result = fib(35)
    elapsed = sys.time() - start
    os.output("fib(35) recursive = {result}, elapsed {elapsed}")

// recursively builds and sums a binary tree of depth n
// each node returns 1 + sum(left) + sum(right)
function tree_sum(n)
    if n == 0
        return 0
    return 1 + tree_sum(n - 1) + tree_sum(n - 1)

// benchmark: tree(25) - evaluates 2^26 - 1 = 67 million recursive calls
function tree25()
    start = sys.time()
    result = tree_sum(25)
    elapsed = sys.time() - start
    os.output("tree(25) = {result}, elapsed {elapsed}")

// benchmark: 100 million iterations using counter-based for loop
function for_counter_100m()
    start = sys.time()
    x = 0
    for i = 1, 100000000
        x = x + i
    elapsed = sys.time() - start
    os.output("100000000 iterations for counter: {x}, elapsed {elapsed}")

// benchmark: 100 million iterations using condition-based for loop
function for_condition_100m()
    start = sys.time()
    x = 0
    i = 1
    for i < 100000001
        x = x + i
        i = i + 1
    elapsed = sys.time() - start
    os.output("100000000 iterations for condition: {x}, elapsed {elapsed}")

// benchmark: appends 'a' 50k times using string interpolation
function string_building()
    start = sys.time()
    s = "start"
    for i = 1, 50000
        s = "{s}a"
    elapsed = sys.time() - start
    os.output("String len (50000): {string.length(s)}, elapsed {elapsed}")

// benchmark: insert, sum lookup, and remove operations on string keys (500k entries)
function string_key_operations()
    start = sys.time()
    t = []
    
    // insert 500k string-keyed entries
    for i = 1, 500000
        t["key_{i}"] = i * 2
    
    // sum all values via string key lookup
    sum = 0
    for i = 1, 500000
        sum = sum + t["key_{i}"]
    
    // remove first two keys
    table.remove(t, "key_1")
    table.remove(t, "key_2")
    
    has1 = table.has(t, "key_1")
    has3 = table.has(t, "key_3")
    
    elapsed = sys.time() - start
    os.output("String keys: {sum}, Table size: {table.size(t)}, elapsed {elapsed}")

// benchmark: inserts 500k key-value pairs, then sums all values
function hashtable_large()
    start = sys.time()
    t = []
    
    // insert 500k entries with string keys
    for i = 1, 500000
        t["item_{i}"] = i
    
    // sum all values via string key lookup
    sum = 0
    for i = 1, 500000
        sum = sum + t["item_{i}"]
    
    elapsed = sys.time() - start
    os.output("Hashtable large: {sum}, elapsed {elapsed}")

// benchmark: copies a table with 10 million integer entries
function table_copy_10m()
    t = []
    for i = 1, 10000000
        t[i] = i * 2
    
    start = sys.time()
    copy = table.copy(t)
    elapsed = sys.time() - start
    os.output("Copy size: {table.size(copy)}, elapsed {elapsed}")

// benchmark: merges two tables with 5 million entries each (10m total)
function table_merge_10m()
    a = []
    b = []
    for i = 1, 5000000
        a[i] = i
        b[i] = i * 2
    
    start = sys.time()
    merged = table.merge(a, b)
    elapsed = sys.time() - start
    os.output("Merged size: {table.size(merged)}, elapsed {elapsed}")

// benchmark: nested table access with 1000*100*10 = 1 million elements
function table_nested_access()
    start = sys.time()
    root = []
    
    // build 3D nested table structure
    for i = 1, 1000
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 10
                root[i][j][k] = i * j * k
    
    // sum all values via triple nested access
    sum = 0
    for i = 1, 1000
        for j = 1, 100
            for k = 1, 10
                sum = sum + root[i][j][k]
    
    elapsed = sys.time() - start
    os.output("Table nested access: {sum}, elapsed {elapsed}")

// benchmark: creates 1 million temporary tables (10 entries each) and sums their 5th element
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

// benchmark: builds and sums a 100x100x100 3D table (1 million elements)
function nested_3d_loops()
    start = sys.time()
    root = []
    
    // allocate and populate 3D table
    for i = 1, 100
        root[i] = []
        for j = 1, 100
            root[i][j] = []
            for k = 1, 100
                root[i][j][k] = i * j * k
    
    // sum all elements via nested iteration
    sum = 0
    for i = 1, 100
        for j = 1, 100
            for k = 1, 100
                sum = sum + root[i][j][k]
    
    elapsed = sys.time() - start
    os.output("Nested 3D loops: {sum}, elapsed {elapsed}")

// benchmark: tests direct array part access with integer index [1]
function table_int_index()
    start = sys.time()
    t = [10, 20, 30, 40, 50]
    sum = 0
    
    // read from index 10 millions times
    for i = 1, 10000000
        sum = sum + t[1]
    
    // write to index 10 millions times
    for i = 1, 10000000
        t[1] = i
    
    elapsed = sys.time() - start
    os.output("Table int index: {sum}, elapsed {elapsed}")

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

function main()
    // recursion
    fib35()
    tree25()
    
    // loops & iteration
    for_counter_100m()
    for_condition_100m()
    
    // string operations
    string_building()
    string_key_operations()
    
    // table operations
    hashtable_large()
    table_copy_10m()
    table_merge_10m()
    table_nested_access()
    temp_tables()
    nested_3d_loops()
    table_int_index()
    
    // matrix & arithmetic
    matrix_multiplication()

main()