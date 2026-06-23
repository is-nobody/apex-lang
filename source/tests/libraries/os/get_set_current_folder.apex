import os
import sys

function set_current_folder()
    result = os.get_current_folder()
    os.output("Old directory = {result}")

    p = sys.platform()

    if p == "Linux"
        os.set_current_folder("/")
    if p == "macOS"
        os.set_current_folder("/")
    if p == "Windows"
        os.set_current_folder("C:\\")

    result = os.get_current_folder()
    os.output("New directory = {result}")

    result = os.set_current_folder("rthrht/www")
    if result == false
        os.output("Cannot change directory (expected)")

set_current_folder()