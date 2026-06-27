import os

function exists()
    file_exists = os.exists("main.c")
    os.output("main.c exists = {file_exists}, expected true")
    
    no_exists = os.exists("nonexistent.xyz")
    os.output("nonexistent.xyz exists = {no_exists}, expected false")
    
    current_exists = os.exists(".")
    os.output("Current directory exists = {current_exists}, expected true")

exists()