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

function table_checks()
    t = ["name" = "Alice", "age" = 30]
    num = 123
    bool_val = true
    nil_val = none
    
    function test_size(val)
        return table.size(val)
    
    assert_equal(test_size(num), none, "size(number)")
    assert_equal(test_size(bool_val), none, "size(bool)")
    assert_equal(test_size(nil_val), none, "size(none)")
    
    function test_has(val, key)
        return table.has(val, key)
    
    assert_equal(test_has(num, "name"), none, "has(number,str)")
    assert_equal(test_has(bool_val, "name"), none, "has(bool,str)")
    assert_equal(test_has(nil_val, "name"), none, "has(none,str)")
    assert_equal(test_has(t, num), false, "has(table,number)")
    assert_equal(test_has(t, bool_val), false, "has(table,bool)")
    assert_equal(test_has(t, nil_val), false, "has(table,none)")
    assert_equal(test_has(num, bool_val), none, "has(number,bool)")
    assert_equal(test_has(nil_val, nil_val), none, "has(none,none)")
    
    function test_remove(val, key)
        return table.remove(val, key)
    
    assert_equal(test_remove(num, "name"), none, "remove(number,str)")
    assert_equal(test_remove(bool_val, "name"), none, "remove(bool,str)")
    assert_equal(test_remove(nil_val, "name"), none, "remove(none,str)")
    assert_equal(test_remove(t, num), false, "remove(table,number)")
    assert_equal(test_remove(t, bool_val), false, "remove(table,bool)")
    assert_equal(test_remove(t, nil_val), false, "remove(table,none)")
    assert_equal(test_remove(num, bool_val), none, "remove(number,bool)")
    assert_equal(test_remove(nil_val, nil_val), none, "remove(none,none)")
    
    function test_keys(val)
        return table.keys(val)
    
    assert_equal(test_keys(num), none, "keys(number)")
    assert_equal(test_keys(bool_val), none, "keys(bool)")
    assert_equal(test_keys(nil_val), none, "keys(none)")
    
    function test_values(val)
        return table.values(val)
    
    assert_equal(test_values(num), none, "values(number)")
    assert_equal(test_values(bool_val), none, "values(bool)")
    assert_equal(test_values(nil_val), none, "values(none)")
    
    function test_clear(val)
        return table.clear(val)
    
    assert_equal(test_clear(num), none, "clear(number)")
    assert_equal(test_clear(bool_val), none, "clear(bool)")
    assert_equal(test_clear(nil_val), none, "clear(none)")
    
    function test_copy(val)
        return table.copy(val)
    
    assert_equal(test_copy(num), none, "copy(number)")
    assert_equal(test_copy(bool_val), none, "copy(bool)")
    assert_equal(test_copy(nil_val), none, "copy(none)")
    
    function test_merge(a, b)
        return table.merge(a, b)
    
    assert_equal(test_merge(num, t), none, "merge(number,table)")
    assert_equal(test_merge(t, num), none, "merge(table,number)")
    assert_equal(test_merge(bool_val, t), none, "merge(bool,table)")
    assert_equal(test_merge(t, bool_val), none, "merge(table,bool)")
    assert_equal(test_merge(nil_val, t), none, "merge(none,table)")
    assert_equal(test_merge(t, nil_val), none, "merge(table,none)")
    assert_equal(test_merge(num, bool_val), none, "merge(number,bool)")
    assert_equal(test_merge(nil_val, nil_val), none, "merge(none,none)")

table_size()
table_has()
table_keys()
table_values()
table_remove()
table_clear()
table_copy()
table_merge()
table_checks()

if tests_failed == 0
    os.output("[OK] libraries/table.apex")