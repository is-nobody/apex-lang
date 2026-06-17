import os
import files

function rename_file()
    files.create_file("old.txt")
    files.rename_file("old.txt", "new.txt")
    e1 = files.exists("old.txt")
    e2 = files.exists("new.txt")
    os.output("old.txt exists: {e1}, expected false")
    os.output("new.txt exists: {e2}, expected true")
    files.delete_file("new.txt")

function rename_folder()
    files.create_folder("old")
    files.rename_folder("old", "new")
    e1 = files.exists("old")
    e2 = files.exists("new")
    os.output("old exists: {e1}, expected false")
    os.output("new exists: {e2}, expected true")
    files.delete_folder("new")

rename_file()
rename_folder()