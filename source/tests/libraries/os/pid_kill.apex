import os

function kill_self()
    p = os.pid()
    os.output("Current PID is {p}")
    os.output("Goodbye, cruel world")
    os.kill(p)

kill_self()