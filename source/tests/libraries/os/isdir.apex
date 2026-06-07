import os

function isdir()
    result = os.isdir("source")
    os.output("source is directory = {result}, expected true")

    result = os.isdir("notexists")
    os.output("notexists is directory = {result}, expected false")

isdir()