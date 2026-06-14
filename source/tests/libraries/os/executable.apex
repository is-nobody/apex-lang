import os

function executable()
    exe = os.executable()
    if exe == false
        os.output("Error: Could not get executable path")
    else
        os.output("Executable: {exe}")

executable()