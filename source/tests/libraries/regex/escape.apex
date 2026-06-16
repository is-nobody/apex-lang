import regex
import os

function test_escape_special()
    res = regex.escape("hello.world")
    if res == "hello\\.world"
        os.output("PASS: escape() special char")
    else
        os.output("FAIL: escape() special char")

function test_escape_clean()
    res = regex.escape("hello")
    if res == "hello"
        os.output("PASS: escape() clean")
    else
        os.output("FAIL: escape() clean")

test_escape_special()
test_escape_clean()