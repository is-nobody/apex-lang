import os

function architecture()
    arch = os.architecture()
    os.output(arch)

architecture()