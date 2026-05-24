import os

errors = 0

// Test 1: Basic range(1, 6)
index = 1
for i in range(1, 6)
    if index == 1 and i != 1
        errors = errors + 1
        os.output("  ERROR Test 1: position 1 = {i}, expected 1")
    if index == 2 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 1: position 2 = {i}, expected 2")
    if index == 3 and i != 3
        errors = errors + 1
        os.output("  ERROR Test 1: position 3 = {i}, expected 3")
    if index == 4 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 1: position 4 = {i}, expected 4")
    if index == 5 and i != 5
        errors = errors + 1
        os.output("  ERROR Test 1: position 5 = {i}, expected 5")
    index = index + 1

if index != 6
    errors = errors + 1
    os.output("  ERROR Test 1: expected 5 items, got {index - 1}")

// Test 2: range with step (0, 11, 2)
index = 1
for i in range(0, 11, 2)
    if index == 1 and i != 0
        errors = errors + 1
        os.output("  ERROR Test 2: position 1 = {i}, expected 0")
    if index == 2 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 2: position 2 = {i}, expected 2")
    if index == 3 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 2: position 3 = {i}, expected 4")
    if index == 4 and i != 6
        errors = errors + 1
        os.output("  ERROR Test 2: position 4 = {i}, expected 6")
    if index == 5 and i != 8
        errors = errors + 1
        os.output("  ERROR Test 2: position 5 = {i}, expected 8")
    if index == 6 and i != 10
        errors = errors + 1
        os.output("  ERROR Test 2: position 6 = {i}, expected 10")
    index = index + 1

// Test 3: negative step range(5, 0, -1)
index = 1
for i in range(5, 0, -1)
    if index == 1 and i != 5
        errors = errors + 1
        os.output("  ERROR Test 3: position 1 = {i}, expected 5")
    if index == 2 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 3: position 2 = {i}, expected 4")
    if index == 3 and i != 3
        errors = errors + 1
        os.output("  ERROR Test 3: position 3 = {i}, expected 3")
    if index == 4 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 3: position 4 = {i}, expected 2")
    if index == 5 and i != 1
        errors = errors + 1
        os.output("  ERROR Test 3: position 5 = {i}, expected 1")
    index = index + 1

// Test 4: break in for loop
index = 1
for i in range(1, 11)
    if i == 5
        break
    if index == 1 and i != 1
        errors = errors + 1
        os.output("  ERROR Test 4: position 1 = {i}, expected 1")
    if index == 2 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 4: position 2 = {i}, expected 2")
    if index == 3 and i != 3
        errors = errors + 1
        os.output("  ERROR Test 4: position 3 = {i}, expected 3")
    if index == 4 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 4: position 4 = {i}, expected 4")
    index = index + 1

if index != 5
    errors = errors + 1
    os.output("  ERROR Test 4: expected 4 items, got {index - 1}")

// Test 5: continue in for loop
index = 1
for i in range(1, 6)
    if i == 3
        continue
    if index == 1 and i != 1
        errors = errors + 1
        os.output("  ERROR Test 5: position 1 = {i}, expected 1")
    if index == 2 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 5: position 2 = {i}, expected 2")
    if index == 3 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 5: position 3 = {i}, expected 4")
    if index == 4 and i != 5
        errors = errors + 1
        os.output("  ERROR Test 5: position 4 = {i}, expected 5")
    index = index + 1

if index != 5
    errors = errors + 1
    os.output("  ERROR Test 5: expected 4 items, got {index - 1}")

// Test 6: for loop over existing table
fruits = ("apple", "banana", "cherry")
index = 1
for fruit in fruits
    if index == 1 and fruit != "apple"
        errors = errors + 1
        os.output("  ERROR Test 6: position 1 = {fruit}, expected apple")
    if index == 2 and fruit != "banana"
        errors = errors + 1
        os.output("  ERROR Test 6: position 2 = {fruit}, expected banana")
    if index == 3 and fruit != "cherry"
        errors = errors + 1
        os.output("  ERROR Test 6: position 3 = {fruit}, expected cherry")
    index = index + 1

// Test 7: for loop over empty table
empty = ()
count = 0
for item in empty
    count = count + 1

if count != 0
    errors = errors + 1
    os.output("  ERROR Test 7: empty table should iterate 0 times, but ran {count} times")

// Test 8: nested for loops
index = 1
for i in range(1, 4)
    for j in range(1, 3)
        result = i * j
        if index == 1 and result != 1
            errors = errors + 1
            os.output("  ERROR Test 8: position 1 = {result}, expected 1")
        if index == 2 and result != 2
            errors = errors + 1
            os.output("  ERROR Test 8: position 2 = {result}, expected 2")
        if index == 3 and result != 2
            errors = errors + 1
            os.output("  ERROR Test 8: position 3 = {result}, expected 2")
        if index == 4 and result != 4
            errors = errors + 1
            os.output("  ERROR Test 8: position 4 = {result}, expected 4")
        if index == 5 and result != 3
            errors = errors + 1
            os.output("  ERROR Test 8: position 5 = {result}, expected 3")
        if index == 6 and result != 6
            errors = errors + 1
            os.output("  ERROR Test 8: position 6 = {result}, expected 6")
        index = index + 1

// Test 9: range with single parameter
index = 1
for i in range(5)
    if index == 1 and i != 0
        errors = errors + 1
        os.output("  ERROR Test 9: position 1 = {i}, expected 0")
    if index == 2 and i != 1
        errors = errors + 1
        os.output("  ERROR Test 9: position 2 = {i}, expected 1")
    if index == 3 and i != 2
        errors = errors + 1
        os.output("  ERROR Test 9: position 3 = {i}, expected 2")
    if index == 4 and i != 3
        errors = errors + 1
        os.output("  ERROR Test 9: position 4 = {i}, expected 3")
    if index == 5 and i != 4
        errors = errors + 1
        os.output("  ERROR Test 9: position 5 = {i}, expected 4")
    index = index + 1

// final result
os.output("Total errors in for.apex: {errors}")

if errors == 0
    os.output("All tests in for.apex passed!")
else
    os.output("{errors} test(s) failed in for.apex.")