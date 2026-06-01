import os


// Test 1: Basic range(1, 6)
index = 1
for i = 1, 6
    os.output("  i = {i}")

// Test 2: range with step (0, 11, 2)
index = 1
for i = 0, 11, 2
    os.output("  i = {i}")

// Test 3: negative step range(5, 0, -1)
index = 1
for i = 5, 0, -1
    os.output("  i = {i}")

// Test 4: break in for loop
index = 1
for i = 1, 11
    os.output("  i = {i}")

// Test 5: continue in for loop
index = 1
for i = 1, 6
    os.output("  i = {i}")

// Test 6: for loop over existing table
fruits = ("apple", "banana", "cherry")
index = 1
for fruit in fruits
    os.output("  fruit = {fruit}")

// Test 7: for loop over empty table
empty = ()
count = 0
for item in empty
    count = count + 1

os.output("empty table iterations: {count}")


// Test 8: nested for loops
index = 1
for i = 1, 4
    os.output("  i = {i}")
    for j = 1, 3
        os.output("    j = {j}, i*j = {i * j}")

// Test 9: range with single parameter
index = 1
for i = 5
    os.output("  i = {i}")
