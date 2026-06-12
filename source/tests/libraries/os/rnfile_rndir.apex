import os

function rnfile()
    os.mkfile("old.txt")
    os.rnfile("old.txt", "new.txt")
    e1 = os.exists("old.txt")
    e2 = os.exists("new.txt")
    os.output("old.txt exists: {e1}, expected false")
    os.output("new.txt exists: {e2}, expected true")
    os.rmfile("new.txt")

function rndir()
    os.mkdir("old")
    os.rndir("old", "new")
    e1 = os.exists("old")
    e2 = os.exists("new")
    os.output("old exists: {e1}, expected false")
    os.output("new exists: {e2}, expected true")
    os.rmdir("new")

rnfile()
rndir()