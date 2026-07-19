import os
import table
import sys

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

table_merge_10m()