import os

function platform()
    p = os.platform()
    os.output("Your platform is {p}, expected {os.platform()}")

platform()