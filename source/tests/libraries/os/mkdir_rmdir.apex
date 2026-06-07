import os

function mkdir()
    result = os.mkdir("folder")
    os.output("folder is created = {result}, expected true")

function rmdir()
    result = os.rmdir("folder")
    os.output("folder is deleted = {result}, expected true")

mkdir()
rmdir()