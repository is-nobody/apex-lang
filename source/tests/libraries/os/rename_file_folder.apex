import os

function rename_file()
    os.create_file("old.txt")
    os.rename("old.txt", "new.txt")
    e1 = os.exists("old.txt")
    e2 = os.exists("new.txt")
    os.output("old.txt exists: {e1}, expected false")
    os.output("new.txt exists: {e2}, expected true")
    os.delete("new.txt")

function rename_folder()
    os.create_folder("old")
    os.rename("old", "new")
    e1 = os.exists("old")
    e2 = os.exists("new")
    os.output("old exists: {e1}, expected false")
    os.output("new exists: {e2}, expected true")
    os.delete("new")

rename_file()
rename_folder()