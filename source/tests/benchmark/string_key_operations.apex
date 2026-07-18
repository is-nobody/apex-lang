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
    
    table.remove(t, "key_1")
    table.remove(t, "key_2")
    
    has1 = table.has(t, "key_1")
    has3 = table.has(t, "key_3")
    
    elapsed = os.time() - start
    os.output("String keys: {sum}, Table size: {table.size(t)}, elapsed {elapsed}")

string_key_operations()