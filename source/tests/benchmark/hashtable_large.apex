// source/tests/benchmark/hashtable_large.apex
// Implementation of Large Hashtable Operations Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

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

// execute benchmark
hashtable_large()