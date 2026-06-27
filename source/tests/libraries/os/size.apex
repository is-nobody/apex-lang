import os

function filesize()
    file = os.size("main.c")
    os.output("Size of 'main.c' is {file / 1024} KB")

    file = os.size("notexists")
    os.output("notexists = {file}, expected false")

function foldersize()
    dir = os.size("source")
    os.output("Size of 'source' is {dir / 1024} KB")

    dir = os.size("notexists")
    os.output("notexists = {dir}, expected false")

filesize()
foldersize()