import os
import sys

function tempdir()
    temp_dir = sys.tempdir()
    if temp_dir == false
        os.output("Error: Could not get temp directory")
    else
        os.output("Temp Directory: {temp_dir}")

tempdir()