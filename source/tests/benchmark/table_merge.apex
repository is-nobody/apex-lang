import os
import table

function table_merge_1m()
    a = []
    b = []
    for i = 1, 500000
        a[i] = i
        b[i] = i * 2
    
    start = os.time()
    merged = table.merge(a, b)
    elapsed = os.time() - start
    os.output("Merged size: {table.size(merged)}, expected 1000000")
    os.output("Time: {elapsed}")

function table_merge_10m()
    a = []
    b = []
    for i = 1, 5000000
        a[i] = i
        b[i] = i * 2
    
    start = os.time()
    merged = table.merge(a, b)
    elapsed = os.time() - start
    os.output("Merged size: {table.size(merged)}, expected 10000000")
    os.output("Time: {elapsed}")

table_merge_1m()
table_merge_10m()