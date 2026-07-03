import os
import table

function string_key_operations()
    start = os.time()
    t = []
    
    for i = 1, 500000
        t["key_{i}"] = i * 2
    
    sum = 0
    for i = 1, 500000
        sum = sum + t["key_{i}"]
    
    os.output("Sum string keys: {sum}")
    
    table.remove(t, "key_1")
    table.remove(t, "key_2")
    
    has1 = table.has(t, "key_1")
    has3 = table.has(t, "key_3")
    
    elapsed = os.time() - start
    os.output("Has key_1: {has1}, expected false, Has key_3: {has3}, expected true")
    os.output("Table size: {table.size(t)}, expected 499998")
    os.output("Time: {elapsed} seconds")

string_key_operations()