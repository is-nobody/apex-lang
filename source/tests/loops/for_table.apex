import os

function for_table_basic()
    items = ["apple", "banana", "cherry"]
    os.output("Basic table iteration:")
    for item = items
        os.output(item)
    os.output("Expected: apple banana cherry")

function for_table_keys()
    data = [name = "Alice", age = 30, city = "Wonderland"]
    os.output("Table keys iteration:")
    for key = data
        os.output(key)
    os.output("Expected: Alice 30 Wonderland")

function for_table_empty()
    empty = []
    os.output("Empty table iteration:")
    count = 0
    for item = empty
        count = count + 1
    os.output("Count = {count}, expected 0")

function for_table_with_break()
    items = [10, 20, 30, 40, 50]
    os.output("Table iteration with break at 30:")
    for item = items
        if item == 30
            break
        os.output(item)
    os.output("Expected: 10 20")

function for_table_with_continue()
    items = [1, 2, 3, 4, 5]
    os.output("Table iteration skipping 3:")
    for item = items
        if item == 3
            continue
        os.output(item)
    os.output("Expected: 1 2 4 5")

function for_table_sum()
    numbers = [10, 20, 30]
    sum = 0
    for n = numbers
        sum = sum + n
    os.output("Sum of table = {sum}, expected 60")

function for_table_nested()
    matrix = [[1, 2], [3, 4]]
    os.output("Nested table iteration:")
    for row = matrix
        for val = row
            os.output(val)
    os.output("Expected: 1 2 3 4")

for_table_basic()
for_table_keys()
for_table_empty()
for_table_with_break()
for_table_with_continue()
for_table_sum()
for_table_nested()