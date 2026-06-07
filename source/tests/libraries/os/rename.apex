import os

function rename()
    os.mkfile("old.txt")
    os.rename("old.txt", "new.txt")
    e1 = os.exists("old.txt")
    e2 = os.exists("new.txt")
    os.output("old.txt exists: {e1}, expected false")
    os.output("new.txt exists: {e2}, expected true")
    os.rmfile("new.txt")

rename()