import os

function append()
    os.write("test_append.txt", "line1\n")
    os.append("test_append.txt", "line2\n")
    os.append("test_append.txt", "line3\n")
    content = os.read("test_append.txt")
    os.output("content:\n{content}")
    os.output("has 3 lines: {content == \"line1\nline2\nline3\n\"}, expected true")
    os.delete_file("test_append.txt")

append()