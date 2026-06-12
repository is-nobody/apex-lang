import os

function kill_spawn()
    pid = os.spawn("echo started && while true; do echo still_alive; done")
    os.output("spawned: {pid}")
    os.wait(0.000385)
    result = os.kill(pid)
    os.output("killed: {result}, expected true")
    code = os.waitpid(pid)
    os.output("exit: {code}, expected -1")

function kill_self()
    p = os.pid()
    os.output("Current PID is {p}")
    os.output("Goodbye, cruel world")
    os.kill(p)

kill_spawn()
kill_self()