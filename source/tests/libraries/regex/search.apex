import regex
import os

function test_search_found()
    res = regex.search("world", "hello world")
    if type(res) == "table" and res.group == "world"
        os.output("PASS: search() found match")
    else
        os.output("FAIL: search() found match")

function test_search_not_found()
    res = regex.search("xyz", "hello world")
    if res == false
        os.output("PASS: search() not found returns false")
    else
        os.output("FAIL: search() not found returns false")

function test_search_groups()
    res = regex.search("(\\d+)-(\\d+)", "id: 123-456 end")
    if type(res) == "table" and res.group == "123-456"
        if res.groups.1 == "123" and res.groups.2 == "456"
            os.output("PASS: search() groups correct")
        else
            os.output("FAIL: search() groups incorrect")
    else
        os.output("FAIL: search() groups base failed")

test_search_found()
test_search_not_found()
test_search_groups()