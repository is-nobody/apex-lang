import os

function filetype()
    file = os.filetype("main.c")
    os.output(file)

    file = os.filetype("notexists")
    if file == false
        os.output("Can't find file")

filetype()