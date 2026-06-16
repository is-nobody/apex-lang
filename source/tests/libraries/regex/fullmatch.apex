import regex
import os

function test_fullmatch_exact()
    res = regex.fullmatch("\\d+", "12345")
    if type(res) == "table" and res.group == "12345"
        os.output("PASS: fullmatch() exact")
    else
        os.output("FAIL: fullmatch() exact")

function test_fullmatch_partial_fail()
    res = regex.fullmatch("\\d+", "123abc")
    if res == false
        os.output("PASS: fullmatch() partial fails")
    else
        os.output("FAIL: fullmatch() partial fails")

test_fullmatch_exact()
test_fullmatch_partial_fail()