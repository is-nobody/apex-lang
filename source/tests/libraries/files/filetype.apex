import os
import files

function filetype()
    file = files.filetype("main.c")
    os.output(file)

    file = files.filetype("notexists")
    if file == false
        os.output("Can't find file")

filetype()