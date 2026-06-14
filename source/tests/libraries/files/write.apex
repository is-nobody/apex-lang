import os
import files

function write()
    write_success = files.write("test.txt", "Hello, Friend")
    files.rmfile("test.txt")

function overwrite()
    files.mkfile("overwrite.txt")

    files.write("overwrite.txt", "Original content")
    os.output("First write: {files.read(\"overwrite.txt\")}")

    files.write("overwrite.txt", "After overwrite")
    os.output("After overwrite: {files.read(\"overwrite.txt\")}")

    files.rmfile("overwrite.txt")

write()
overwrite()