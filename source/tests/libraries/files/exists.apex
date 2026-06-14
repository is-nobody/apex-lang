import os
import files

function exists()
    file_exists = files.exists("main.c")
    os.output("main.c exists = {file_exists}, expected true")
    
    no_exists = files.exists("nonexistent.xyz")
    os.output("nonexistent.xyz exists = {no_exists}, expected false")
    
    current_exists = files.exists(".")
    os.output("Current directory exists = {current_exists}, expected true")

exists()