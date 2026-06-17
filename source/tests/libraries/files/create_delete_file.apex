import os
import files

function create_file()
    result = files.create_file("file")
    os.output("file is created = {result}, expected true")

function delete_file()
    result = files.delete_file("file")
    os.output("file is deleted = {result}, expected true")

create_file()
delete_file()