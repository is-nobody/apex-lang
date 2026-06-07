import os

function time()
    t = os.time()
    os.output("Time = {t}, expected {os.time()}")

time()