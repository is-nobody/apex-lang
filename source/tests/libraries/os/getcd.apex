import os

function getcd()
    result = os.getcd()
    os.output("getcd() = {result}, expected {os.getcd()}")

getcd()