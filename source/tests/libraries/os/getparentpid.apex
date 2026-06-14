import os

function getparentpid()
    ppid = os.getparentpid()
    my_pid = os.pid()

    os.output("My PID: {my_pid}")
    os.output("Parent PID: {ppid}")

    if ppid > 0
        os.output("Success: Parent PID is valid")
    else
        os.output("Warning: Parent PID is 0 or invalid")

getparentpid()