import random
import os

a = "secret_token_123"
b = "secret_token_123"
res = random.compare_digest(a, b)
if res == true
    os.output("PASS: compare_digest matches identical strings")
else
    os.output("FAIL: compare_digest should match identical strings")