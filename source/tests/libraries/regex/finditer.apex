import regex
import os
import table

function test_finditer_structure()
    res = regex.finditer("\\w+", "hi there")
    if type(res) == "table" and table.size(res) == 2
        first = res.1
        if type(first) == "table" and first.group == "hi"
            os.output("PASS: finditer() structure")
        else
            os.output("FAIL: finditer() first item")
    else
        os.output("FAIL: finditer() size")

test_finditer_structure()