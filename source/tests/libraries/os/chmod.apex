import os

function chmod()
    os.write("test_chmod.txt", "test")
    result = os.chmod("test_chmod.txt", 438)
    os.output("chmod 438: {result}, expected true")
    stat = os.stat("test_chmod.txt")
    os.output("file exists: {stat != false}, expected true")
    os.output("size: {stat.size}, expected 4")
    os.rmfile("test_chmod.txt")
    os.output("chmod nonexist: {os.chmod(\"no_file.txt\", 438)}, expected false")

chmod()