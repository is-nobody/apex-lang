import os

function setcd()
    result = os.getcd()
    os.output("Old directory = {result}")

    p = os.platform()

    if p == "Linux"
        os.setcd("/")
    if p == "macOS"
        os.setcd("/")
    if p == "Windows"
        os.setcd("C:\\")

    result = os.getcd()
    os.output("New directory = {result}")

    result = os.setcd("rthrht/www")
    if result == false
        os.output("Cannot change directory")

setcd()