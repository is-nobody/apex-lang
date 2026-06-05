import os

function string_equal_false()
    result = "hello" == "world"
    os.output("hello == world = {result}, expected false")

function string_equal_true()
    result = "hello" == "hello"
    os.output("hello == hello = {result}, expected true")

function string_not_equal_false()
    result = "hello" != "hello"
    os.output("hello != hello = {result}, expected false")

function string_not_equal_true()
    result = "hello" != "world"
    os.output("hello != world = {result}, expected true")
    
string_equal_false()
string_equal_true()
string_not_equal_false()
string_not_equal_true()