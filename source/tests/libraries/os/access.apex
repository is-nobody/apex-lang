import os

function access()
    os.write("test_access.txt", "test")
    result = os.access("test_access.txt", 438)
    os.output("access 438: {result}, expected true")
    stat = os.stat("test_access.txt")
    os.output("file exists: {stat != false}, expected true")
    os.output("size: {stat[\"size\"]}, expected 4")
    os.delete("test_access.txt")
    os.output("access nonexist: {os.access(\"no_file.txt\", 438)}, expected false")

access()