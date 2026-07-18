import os
import string
import sys

function string_building()
    start = sys.time()
    s = "start"
    for i = 1, 50000
        s = "{s}a"
    elapsed = sys.time() - start
    os.output("String len (50000): {string.length(s)}, elapsed {elapsed}")

string_building()