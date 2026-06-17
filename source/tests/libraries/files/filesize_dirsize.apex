import os
import files

function filesize()
    file = files.filesize("main.c")
    os.output("Size of 'main.c' is {file / 1024} KB")

    file = files.filesize("notexists")
    os.output("notexists = {file}, expected false")

function foldersize()
    dir = files.foldersize("source")
    os.output("Size of 'source' is {dir / 1024} KB")

    dir = files.foldersize("notexists")
    os.output("notexists = {dir}, expected false")

filesize()
foldersize()