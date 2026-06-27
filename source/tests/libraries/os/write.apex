import os

function write()
    write_success = os.write("test.txt", "Hello, Friend")
    os.delete("test.txt")

function overwrite()
    os.create_file("overwrite.txt")

    os.write("overwrite.txt", "Original content")
    os.output("First write: {os.read(\"overwrite.txt\")}")

    os.write("overwrite.txt", "After overwrite")
    os.output("After overwrite: {os.read(\"overwrite.txt\")}")

    os.delete("overwrite.txt")

write()
overwrite()