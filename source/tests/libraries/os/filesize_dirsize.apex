import os

function filesize()
    file = os.filesize("main.c")
    os.output("Size of 'main.c' is {file / 1024} KB")

    file = os.filesize("notexists")
    os.output("notexists = {file}, expected false")

function foldersize()
    dir = os.foldersize("source")
    os.output("Size of 'source' is {dir / 1024} KB")

    dir = os.foldersize("notexists")
    os.output("notexists = {dir}, expected false")

filesize()
foldersize()