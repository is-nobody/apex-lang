import os
import files

function read()
    content = files.read(".gitignore")
    
    if content == false
        os.output("Failed to read file")
    else
        os.output(content)

read()