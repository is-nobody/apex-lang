import os
import sys

function check(name, actual, expected)
    if actual == expected
        os.output("[OK] {name}")
    else
        os.output("[FAIL] {name}: got {actual}, expected {expected}")

isterminal = sys.isterminal()
check("sys.isterminal", isterminal, true)