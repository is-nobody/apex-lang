import os

function read()
    content = os.read(".gitignore")
    
    if content == false
        os.output("Failed to read file")
    else
        os.output(content)

read()