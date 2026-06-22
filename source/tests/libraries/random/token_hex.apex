import random
import os
import string

nbytes = 16
hex_str = random.secure_token_hex(nbytes)
if string.len(hex_str) == nbytes * 2
    os.output("PASS: secure_token_hex(16) returns 32 char string")
else
    os.output("FAIL: secure_token_hex(16) wrong length")