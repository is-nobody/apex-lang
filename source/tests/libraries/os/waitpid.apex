import os

function waitpid()
    pid = os.spawn("echo test")
    os.output("spawn pid: {pid}")
    exit_code = os.waitpid(pid)
    os.output("exit code: {exit_code}, expected 0")
    os.output("success: {exit_code == 0}, expected true")
    fail_pid = os.spawn("nonexist_command_xyz")
    fail_code = os.waitpid(fail_pid)
    os.output("fail exit: {fail_code != 0}, expected true")

waitpid()