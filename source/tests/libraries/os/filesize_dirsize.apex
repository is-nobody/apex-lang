import os
import files

function filesize()
    file = files.filesize("main.c")
    os.output("Size of 'main.c' is {file}")

    file = files.filesize("notexists")
    os.output("notexists = {file}, expected false")
    if file == false
        os.output("Can't find file")

function dirsize()
    dir = files.dirsize("source")
    os.output("Size of 'source' is {dir}")

    dir = files.dirsize("notexists")
    os.output("notexists = {dir}, expected false")
    if dir == false
        os.output("Can't find directory")

filesize()
dirsize()