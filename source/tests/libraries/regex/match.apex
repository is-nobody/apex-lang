import regex
import os

function test_match_start()
    res = regex.match("hello", "hello world")
    if type(res) == "table" and res.group == "hello"
        os.output("PASS: match() at start")
    else
        os.output("FAIL: match() at start")

function test_match_middle_fail()
    res = regex.match("world", "hello world")
    if res == false
        os.output("PASS: match() middle fails")
    else
        os.output("FAIL: match() middle fails")

test_match_start()
test_match_middle_fail()