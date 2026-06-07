import os

function write()
    write_success = os.write("test.txt", "Hello, Friend")
    
    if write_success == false
        os.output("Failed to write file")

    os.rimfile("text.txt")

function overwrite()
    os.mkfile("overwrite.txt")

    os.write("overwrite.txt", "Original content")
    os.output("First write: {os.read("overwrite.txt")}")

    os.write("overwrite.txt", "After overwrite")
    os.output("After overwrite: {os.read("overwrite.txt")}")

    os.rmfile("overwrite.txt")

write()
overwrite()