import os
import table

function table_copy_1m()
    t = []
    for i = 1, 1000000
        t[i] = i * 2
    
    start = os.time()
    copy = table.copy(t)
    elapsed = os.time() - start
    os.output("Copy size: {table.size(copy)}, expected 10000000")
    os.output("Time: {elapsed}")

function table_copy_10m()
    t = []
    for i = 1, 10000000
        t[i] = i * 2
    
    start = os.time()
    copy = table.copy(t)
    elapsed = os.time() - start
    os.output("Copy size: {table.size(copy)}, expected 10000000")
    os.output("Time: {elapsed}")


function table_copy_100m()
    t = []
    for i = 1, 100000000
        t[i] = i * 2
    
    start = os.time()
    copy = table.copy(t)
    elapsed = os.time() - start
    os.output("Copy size: {table.size(copy)}, expected 100000000")
    os.output("Time: {elapsed}")

table_copy_1m()
table_copy_10m()
table_copy_100m()