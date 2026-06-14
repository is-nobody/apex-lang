import os
import files

function rnfile()
    files.mkfile("old.txt")
    files.rnfile("old.txt", "new.txt")
    e1 = files.exists("old.txt")
    e2 = files.exists("new.txt")
    os.output("old.txt exists: {e1}, expected false")
    os.output("new.txt exists: {e2}, expected true")
    files.rmfile("new.txt")

function rndir()
    files.mkdir("old")
    files.rndir("old", "new")
    e1 = files.exists("old")
    e2 = files.exists("new")
    os.output("old exists: {e1}, expected false")
    os.output("new exists: {e2}, expected true")
    files.rmdir("new")

rnfile()
rndir()