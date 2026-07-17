import os

function hashtable_large()
    start = os.time()
    t = []
    
    for i = 1, 500000
        t["item_{i}"] = i
    
    sum = 0
    for i = 1, 500000
        sum = sum + t["item_{i}"]
    
    elapsed = os.time() - start
    os.output("Hashtable large: {sum}, elapsed {elapsed}")

hashtable_large()