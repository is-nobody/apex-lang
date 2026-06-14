import os
import sys

function executable()
    exe = sys.executable()
    if exe == false
        os.output("Error: Could not get executable path")
    else
        os.output("Executable: {exe}")

executable()