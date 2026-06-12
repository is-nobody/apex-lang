import os

function getenv()
    user = os.getenv("USER")
    path = os.getenv("PATH")
    fake = os.getenv("NONEXIST_VAR_12345")
    os.output("USER: {user}, expected {os.getenv(\"USER\")}")
    os.output("PATH exists: {path != false}, expected true")
    os.output("NONEXIST: {fake}, expected false")

getenv()