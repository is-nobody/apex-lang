import os

function mkfile()
    result = os.mkdir("file")
    os.output("file is created = {result}, expected true")

function rmfile()
    result = os.rmdir("file")
    os.output("file is deleted = {result}, expected true")

mkfile()
rmfile()