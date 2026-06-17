import os
import files

function isfolder()
    result = files.isfolder("source")
    os.output("source is folder = {result}, expected true")

    result = files.isfolder("notexists")
    os.output("notexists is folder = {result}, expected false")

isfolder()