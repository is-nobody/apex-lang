import os
import files

function mkfile()
    result = files.mkdir("file")
    os.output("file is created = {result}, expected true")

function rmfile()
    result = files.rmdir("file")
    os.output("file is deleted = {result}, expected true")

mkfile()
rmfile()