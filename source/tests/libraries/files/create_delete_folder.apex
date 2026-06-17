import os
import files

function create_folder()
    result = files.create_folder("folder")
    os.output("folder is created = {result}, expected true")

function delete_folder()
    result = files.delete_folder("folder")
    os.output("folder is deleted = {result}, expected true")

create_folder()
delete_folder()