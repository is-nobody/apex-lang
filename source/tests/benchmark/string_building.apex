// source/tests/benchmark/string_building.apex
// Implementation of String Building Benchmark for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string
import sys

// benchmark: appends 'a' 1m times using string interpolation
function string_building()
    start = sys.time()
    s = "start"
    for i = 1, 1000000
        s = "{s}a"
    elapsed = sys.time() - start
    os.output("String len (1000000): {string.length(s)}, elapsed {elapsed}")

// execute benchmark
string_building()