import os
import string

function string_building()
    start = os.time()
    s = "start"
    for i = 1, 50000
        s = "{s}a"
    elapsed = os.time() - start
    os.output("String len (50000): {string.length(s)}, expected 50005")
    os.output("Time: {elapsed} seconds")

string_building()