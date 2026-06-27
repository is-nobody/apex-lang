import os

function isfolder()
    result = os.isfolder("source")
    os.output("source is folder = {result}, expected true")

    result = os.isfolder("notexists")
    os.output("notexists is folder = {result}, expected false")

isfolder()