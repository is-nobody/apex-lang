import os
import table

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function table_size()
    t = [1, 2, 3]
    size = table.size(t)
    assert_equal(size, 3, "table.size")

function table_has()
    t = ["name" = "Alice", "age" = 30]
    has_name = table.has(t, "name")
    assert_equal(has_name, true, "table.has name")
    has_city = table.has(t, "city")
    assert_equal(has_city, false, "table.has city")

function table_keys()
    t = ["name" = "Alice", "age" = 30]
    keys = table.keys(t)
    is_table = type(keys) == "table"
    assert_equal(is_table, true, "table.keys returns table")
    if is_table == true
        size = table.size(keys)
        assert_equal(size, 2, "table.keys count")

function table_values()
    t = ["name" = "Alice", "age" = 30]
    values = table.values(t)
    is_table = type(values) == "table"
    assert_equal(is_table, true, "table.values returns table")
    if is_table == true
        size = table.size(values)
        assert_equal(size, 2, "table.values count")

function table_remove()
    t = ["name" = "Alice", "age" = 30, "city" = "Dubai"]
    size_before = table.size(t)
    assert_equal(size_before, 3, "table.remove size before")
    table.remove(t, "age")
    size_after = table.size(t)
    assert_equal(size_after, 2, "table.remove size after")
    has_name = table.has(t, "name")
    has_age = table.has(t, "age")
    assert_equal(has_name, true, "table.remove name remains")
    assert_equal(has_age, false, "table.remove age removed")

function table_clear()
    t = [1, 2, 3]
    table.clear(t)
    size = table.size(t)
    assert_equal(size, 0, "table.clear")

function table_copy()
    original = ["x" = 1, "y" = 2]
    copy = table.copy(original)
    copy["x"] = 999
    assert_equal(original["x"], 1, "table.copy original unchanged")
    assert_equal(copy["x"], 999, "table.copy copy changed")
    assert_equal(copy["y"], 2, "table.copy shared value")

function table_merge()
    a = ["x" = 1, "y" = 2]
    b = ["y" = 999, "z" = 3]
    m = table.merge(a, b)
    is_table = type(m) == "table"
    assert_equal(is_table, true, "table.merge returns table")
    if is_table == true
        size = table.size(m)
        assert_equal(size, 3, "table.merge size")
        assert_equal(m["x"], 1, "table.merge keep x")
        assert_equal(m["y"], 999, "table.merge overwrite y")
        assert_equal(m["z"], 3, "table.merge add z")

table_size()
table_has()
table_keys()
table_values()
table_remove()
table_clear()
table_copy()
table_merge()

if tests_failed == 0
    os.output("[OK] libraries/table.apex")