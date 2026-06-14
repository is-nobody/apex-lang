import os
import files

function isfile()
    result = files.isfile("main.c")
    os.output("main.c is file = {result}, expected true")

    result = files.isfile("notexists")
    os.output("notexists is file = {result}, expected false")

isfile()