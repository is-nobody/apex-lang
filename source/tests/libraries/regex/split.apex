import regex
import os
import table

function test_split_simple()
    res = regex.split(",", "a,b,c")
    if type(res) == "table" and table.size(res) == 3
        if res.1 == "a" and res.2 == "b" and res.3 == "c"
            os.output("PASS: split() simple")
        else
            os.output("FAIL: split() values")
    else
        os.output("FAIL: split() size")

function test_split_regex()
    res = regex.split("\\s+", "one  two   three")
    if type(res) == "table" and table.size(res) == 3
        os.output("PASS: split() regex")
    else
        os.output("FAIL: split() regex")

test_split_simple()
test_split_regex()