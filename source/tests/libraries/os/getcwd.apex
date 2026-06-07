import os

function getcwd()
    result = os.getcwd()
    os.output("getcwd() = {result}, expected {os.getcwd()}")

getcwd()