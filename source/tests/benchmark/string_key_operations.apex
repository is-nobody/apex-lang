// source/tests/benchmark/string_key_operations.apex
// Implementation of String Key Operations Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import table
import sys

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

// execute benchmark
string_key_operations()