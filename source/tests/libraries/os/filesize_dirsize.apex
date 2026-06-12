import os

function filesize()
    file = os.filesize("main.c")
    os.output(file)

    file = os.filesize("notexists")
    os.output("{file}, expected false")
    if file == false
        os.output("Can't find file")

function dirsize()
    dir = os.dirsize("source")
    os.output(dir)

    dir = os.dirsize("notexists")
    os.output("{dir}, expected false")
    if dir == false
        os.output("Can't find directory")

filesize()
dirsize()