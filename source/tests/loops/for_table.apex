// for_table.apex
import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function for_table_basic()
    result = ""
    items = ["apple", "banana", "cherry"]
    for key = items
        result = "{result}{items[key]}"
    assert_equal(result, "applebananacherry", "basic table iteration")

function for_table_keys()
    result = ""
    data = ["name" = "Alice", "age" = 30, "city" = "Wonderland"]
    for key = data
        result = "{result}{data[key]}"
    assert_equal(result, "30WonderlandAlice", "table keys iteration")

function for_table_empty()
    empty = []
    count = 0
    for key = empty
        count = count + 1
    assert_equal(count, 0, "empty table iteration")

function for_table_with_break()
    result = ""
    items = [10, 20, 30, 40, 50]
    for key = items
        if items[key] == 30
            break
        result = "{result}{items[key]}"
    assert_equal(result, "1020", "table break at 30")

function for_table_with_continue()
    result = ""
    items = [1, 2, 3, 4, 5]
    for key = items
        if items[key] == 3
            continue
        result = "{result}{items[key]}"
    assert_equal(result, "1245", "table skip 3")

function for_table_sum()
    numbers = [10, 20, 30]
    sum = 0
    for key = numbers
        sum = sum + numbers[key]
    assert_equal(sum, 60, "sum of table")

function for_table_nested()
    result = ""
    matrix = [[1, 2], [3, 4]]
    for row_key = matrix
        for col_key = matrix[row_key]
            result = "{result}{matrix[row_key][col_key]}"
    assert_equal(result, "1234", "nested table iteration")

for_table_basic()
for_table_keys()
for_table_empty()
for_table_with_break()
for_table_with_continue()
for_table_sum()
for_table_nested()

if tests_failed == 0
    os.output("[OK] loops/for_table.apex")