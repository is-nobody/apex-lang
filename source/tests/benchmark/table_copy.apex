import os
import table
import sys

function table_copy_1m()
    t = []
    for i = 1, 1000000
        t[i] = i * 2
    
    start = sys.time()
    copy = table.copy(t)
    elapsed = sys.time() - start
    os.output("Copy size: {table.size(copy)}, elapsed {elapsed}")

function table_copy_10m()
    t = []
    for i = 1, 10000000
        t[i] = i * 2
    
    start = sys.time()
    copy = table.copy(t)
    elapsed = sys.time() - start
    os.output("Copy size: {table.size(copy)}, elapsed {elapsed}")

table_copy_1m()
table_copy_10m()