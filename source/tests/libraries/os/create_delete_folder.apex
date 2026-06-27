import os

function create_folder()
    result = os.create_folder("folder")
    os.output("folder is created = {result}, expected true")

function delete_folder()
    result = os.delete_folder("folder")
    os.output("folder is deleted = {result}, expected true")

create_folder()
delete_folder()