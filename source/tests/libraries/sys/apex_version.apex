import os
import sys

function apex_version()
    version = sys.apex_version()
    if version == false
        os.output("Error: Could not get executable path")
    else
        os.output("Apex version: {version}")

apex_version()