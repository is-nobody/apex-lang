import os
import files

function append()
    files.write("test_append.txt", "line1\n")
    files.append("test_append.txt", "line2\n")
    files.append("test_append.txt", "line3\n")
    content = files.read("test_append.txt")
    os.output("content:\n{content}")
    os.output("has 3 lines: {content == \"line1\nline2\nline3\n\"}, expected true")
    files.rmfile("test_append.txt")

append()