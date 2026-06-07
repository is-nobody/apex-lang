import os

function isfile()
    result = os.isfile("main.c")
    os.output("main.c is file = {result}, expected true")

    result = os.isfile("notexists")
    os.output("notexists is file = {result}, expected false")

isfile()