import os
import files

function mkdir()
    result = files.mkdir("folder")
    os.output("folder is created = {result}, expected true")

function rmdir()
    result = files.rmdir("folder")
    os.output("folder is deleted = {result}, expected true")

mkdir()
rmdir()