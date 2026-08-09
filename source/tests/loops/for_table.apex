// source/tests/loops/for_table.apex
// Implementation of Table For Loop Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: basic iteration over table values
function for_table_basic()
    result = ""
    items = ["apple", "banana", "cherry"]
    for value = items
        result = "{result}{value}"
    assert_equal(result, "applebananacherry", "basic table iteration")

// test: iteration over table with mixed keys (string keys)
function for_table_keys()
    result = ""
    data = ["name" = "Alice", "age" = 30, "city" = "Wonderland"]
    for value = data
        result = "{result}{value}"
    assert_equal(result, "30WonderlandAlice", "table keys iteration")

// test: iteration over empty table yields zero iterations
function for_table_empty()
    empty = []
    count = 0
    for value = empty
        count = count + 1
    assert_equal(count, 0, "empty table iteration")

// test: break exits table iteration early
function for_table_with_break()
    result = ""
    items = [10, 20, 30, 40, 50]
    for value = items
        if value == 30
            break
        result = "{result}{value}"
    assert_equal(result, "1020", "table break at 30")

// test: continue skips current table value
function for_table_with_continue()
    result = ""
    items = [1, 2, 3, 4, 5]
    for value = items
        if value == 3
            continue
        result = "{result}{value}"
    assert_equal(result, "1245", "table skip 3")

// test: accumulator pattern over table values
function for_table_sum()
    numbers = [10, 20, 30]
    sum = 0
    for value = numbers
        sum = sum + value
    assert_equal(sum, 60, "sum of table")

// test: nested table iteration (matrix)
function for_table_nested()
    result = ""
    matrix = [[1, 2], [3, 4]]
    for row = matrix
        for value = row
            result = "{result}{value}"
    assert_equal(result, "1234", "nested table iteration")

// execute all table for loop test cases
for_table_basic()
for_table_keys()
for_table_empty()
for_table_with_break()
for_table_with_continue()
for_table_sum()
for_table_nested()

// report completion of table for loop tests
os.output("[PASS] loops/for_table.apex")