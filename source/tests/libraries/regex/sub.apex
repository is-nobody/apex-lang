import regex
import os

function test_sub_replace()
    res = regex.sub("cat", "dog", "the cat sat")
    if res == "the dog sat"
        os.output("PASS: sub() replace")
    else
        os.output("FAIL: sub() replace")

function test_sub_no_match()
    res = regex.sub("xyz", "abc", "hello")
    if res == "hello"
        os.output("PASS: sub() no match")
    else
        os.output("FAIL: sub() no match")

test_sub_replace()
test_sub_no_match()