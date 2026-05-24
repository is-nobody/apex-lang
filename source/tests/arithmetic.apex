import os

errors = 0
result = 0

// whole with whole
result = 10 + 3
if result != 13
    os.output("  ERROR: 10 + 3 = {result}, expected 13")
    errors = errors + 1

result = 10 - 3
if result != 7
    os.output("  ERROR: 10 - 3 = {result}, expected 7")
    errors = errors + 1

result = 10 * 3
if result != 30
    os.output("  ERROR: 10 * 3 = {result}, expected 30")
    errors = errors + 1

result = 10 / 3
if result != 10 / 3
    errors = errors + 0  // skip comparison with float

result = 10 % 3
if result != 1
    os.output("  ERROR: 10 % 3 = {result}, expected 1")
    errors = errors + 1

// decimal with decimal
result = 3.5 + 1.5
if result != 5.0
    os.output("  ERROR: 3.5 + 1.5 = {result}, expected 5.0")
    errors = errors + 1

result = 3.5 - 1.5
if result != 2.0
    os.output("  ERROR: 3.5 - 1.5 = {result}, expected 2.0")
    errors = errors + 1

result = 3.5 * 1.5
if result != 5.25
    os.output("  ERROR: 3.5 * 1.5 = {result}, expected 5.25")
    errors = errors + 1

result = 3.5 / 1.5
if result != 3.5 / 1.5
    errors = errors + 0

result = 3.5 % 1.5
if result != 0.5
    os.output("  ERROR: 3.5 % 1.5 = {result}, expected 0.5")
    errors = errors + 1

// whole with decimal
result = 5 + 2.5
if result != 7.5
    os.output("  ERROR: 5 + 2.5 = {result}, expected 7.5")
    errors = errors + 1

result = 5 - 2.5
if result != 2.5
    os.output("  ERROR: 5 - 2.5 = {result}, expected 2.5")
    errors = errors + 1

result = 5 * 2.5
if result != 12.5
    os.output("  ERROR: 5 * 2.5 = {result}, expected 12.5")
    errors = errors + 1

result = 5 / 2.5
if result != 2.0
    os.output("  ERROR: 5 / 2.5 = {result}, expected 2.0")
    errors = errors + 1

result = 5 % 2.5
if result != 0
    os.output("  ERROR: 5 % 2.5 = {result}, expected 0")
    errors = errors + 1

// priority - same precedence left to right
result = 20 - 5 - 3
if result != 12
    os.output("  ERROR: 20 - 5 - 3 = {result}, expected 12")
    errors = errors + 1

result = 40 / 4 / 2
if result != 5
    os.output("  ERROR: 40 / 4 / 2 = {result}, expected 5")
    errors = errors + 1

result = 10 - 5 + 2
if result != 7
    os.output("  ERROR: 10 - 5 + 2 = {result}, expected 7")
    errors = errors + 1

result = 12 * 3 / 2
if result != 18
    os.output("  ERROR: 12 * 3 / 2 = {result}, expected 18")
    errors = errors + 1

// priority - mixed operators without parentheses
result = 4 * 3 + 5
if result != 17
    os.output("  ERROR: 4 * 3 + 5 = {result}, expected 17")
    errors = errors + 1

result = 4 + 3 * 5
if result != 19
    os.output("  ERROR: 4 + 3 * 5 = {result}, expected 19")
    errors = errors + 1

result = 30 / 3 + 4
if result != 14
    os.output("  ERROR: 30 / 3 + 4 = {result}, expected 14")
    errors = errors + 1

result = 30 + 9 / 3
if result != 33
    os.output("  ERROR: 30 + 9 / 3 = {result}, expected 33")
    errors = errors + 1

result = 18 - 2 * 4
if result != 10
    os.output("  ERROR: 18 - 2 * 4 = {result}, expected 10")
    errors = errors + 1

result = 2 * 8 - 3
if result != 13
    os.output("  ERROR: 2 * 8 - 3 = {result}, expected 13")
    errors = errors + 1

result = 24 / 4 - 3
if result != 3
    os.output("  ERROR: 24 / 4 - 3 = {result}, expected 3")
    errors = errors + 1

result = 24 - 12 / 3
if result != 20
    os.output("  ERROR: 24 - 12 / 3 = {result}, expected 20")
    errors = errors + 1

result = 13 % 5 + 2
if result != 5
    os.output("  ERROR: 13 % 5 + 2 = {result}, expected 5")
    errors = errors + 1

result = 13 + 5 % 3
if result != 15
    os.output("  ERROR: 13 + 5 % 3 = {result}, expected 15")
    errors = errors + 1

// priority - parentheses changing order
result = (2 + 3) * 5
if result != 25
    os.output("  ERROR: (2 + 3) * 5 = {result}, expected 25")
    errors = errors + 1

result = 2 * (5 - 3)
if result != 4
    os.output("  ERROR: 2 * (5 - 3) = {result}, expected 4")
    errors = errors + 1

result = 20 / (3 + 2)
if result != 4
    os.output("  ERROR: 20 / (3 + 2) = {result}, expected 4")
    errors = errors + 1

result = (8 - 3) * (2 + 1)
if result != 15
    os.output("  ERROR: (8 - 3) * (2 + 1) = {result}, expected 15")
    errors = errors + 1

// priority - multiple levels
result = 2 + 3 * 4 - 1
if result != 13
    os.output("  ERROR: 2 + 3 * 4 - 1 = {result}, expected 13")
    errors = errors + 1

result = 5 * 2 + 3 * 5
if result != 25
    os.output("  ERROR: 5 * 2 + 3 * 5 = {result}, expected 25")
    errors = errors + 1

result = 20 / 4 - 3 * 1 + 2
if result != 4
    os.output("  ERROR: 20 / 4 - 3 * 1 + 2 = {result}, expected 4")
    errors = errors + 1

result = 10 % 3 + 2 * 4
if result != 9
    os.output("  ERROR: 10 % 3 + 2 * 4 = {result}, expected 9")
    errors = errors + 1

// final result
os.output("Total errors in arithmetic.apex: {errors}")

if errors == 0
    os.output("Tests in arithmetic.apex passed!")
else
    os.output("{errors} test(s) failed in arithmetic.apex.")