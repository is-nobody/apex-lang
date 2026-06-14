import os
import files

function isdir()
    result = files.isdir("source")
    os.output("source is directory = {result}, expected true")

    result = files.isdir("notexists")
    os.output("notexists is directory = {result}, expected false")

isdir()