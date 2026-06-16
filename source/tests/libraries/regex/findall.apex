import regex
import os
import table

function test_findall_multiple()
    res = regex.findall("\\d+", "a1 b2 c3")
    if type(res) == "table" and table.size(res) == 3
        if res.1 == "1" and res.2 == "2" and res.3 == "3"
            os.output("PASS: findall() multiple")
        else
            os.output("FAIL: findall() values incorrect")
    else
        os.output("FAIL: findall() size incorrect")

function test_findall_none()
    res = regex.findall("\\d+", "abc")
    if type(res) == "table" and table.size(res) == 0
        os.output("PASS: findall() none")
    else
        os.output("FAIL: findall() none")

test_findall_multiple()
test_findall_none()