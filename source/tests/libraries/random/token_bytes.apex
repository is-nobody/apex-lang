import random
import os
import string

n = 32
bytes_str = random.secure_token_bytes(n)
if string.len(bytes_str) == n
    os.output("PASS: secure_token_bytes(32) returns 32 byte string")
else
    os.output("FAIL: secure_token_bytes(32) wrong length")