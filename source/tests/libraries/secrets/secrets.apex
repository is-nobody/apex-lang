import secrets
import os
// 1. secrets.token_bytes
tb = secrets.token_bytes(16)
if type(tb) == "string" and string.len(tb) == 16
    os.output("PASS: token_bytes")
else
    os.output("FAIL: token_bytes")

// 2. secrets.token_hex
th = secrets.token_hex(8)
if type(th) == "string" and string.len(th) == 16
    os.output("PASS: token_hex")
else
    os.output("FAIL: token_hex")

// Default arg check
th_def = secrets.token_hex()
if string.len(th_def) == 32
    os.output("PASS: token_hex default")
else
    os.output("FAIL: token_hex default")

// 3. secrets.token_urlsafe
tu = secrets.token_urlsafe(12)
if type(tu) == "string" and string.len(tu) >= 12
    os.output("PASS: token_urlsafe")
else
    os.output("FAIL: token_urlsafe")

// 4. secrets.choice
items = ("a", "b", "c", "d")
ch = secrets.choice(items)

found = false
for k = items
    if k == ch
        found = true

if found == true
    os.output("PASS: choice")
else
    os.output("FAIL: choice")

// Choice failure
empty = ()
if secrets.choice(empty) == false
    os.output("PASS: choice empty")
else
    os.output("FAIL: choice empty")

// 5. secrets.randbelow
rb = secrets.randbelow(100)
if type(rb) == "number" and rb >= 0 and rb < 100
    os.output("PASS: randbelow")
else
    os.output("FAIL: randbelow")

if secrets.randbelow(0) == false
    os.output("PASS: randbelow zero")
else
    os.output("FAIL: randbelow zero")

// 6. secrets.randbits
rbi = secrets.randbits(8)
if type(rbi) == "number" and rbi >= 0 and rbi <= 255
    os.output("PASS: randbits")
else
    os.output("FAIL: randbits")

// 7. secrets.compare_digest
if secrets.compare_digest("test", "test") == true and secrets.compare_digest("test", "fail") == false
    os.output("PASS: compare_digest")
else
    os.output("FAIL: compare_digest")

if secrets.compare_digest("abc", "abcd") == false
    os.output("PASS: compare_digest len")
else
    os.output("FAIL: compare_digest len")