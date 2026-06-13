import os

function check(name, actual, expected)
    if actual == expected
        os.output("[OK] {name}")
    else
        os.output("[FAIL] {name}: got {actual}, expected {expected}")

stdin_info = os.stdin()
check("os.stdin.handle", stdin_info.handle, 0)
check("os.stdin.mode", stdin_info.mode, "r")

stdout_info = os.stdout()
check("os.stdout.handle", stdout_info.handle, 1)
check("os.stdout.mode", stdout_info.mode, "w")

stderr_info = os.stderr()
check("os.stderr.handle", stderr_info.handle, 2)
check("os.stderr.mode", stderr_info.mode, "w")