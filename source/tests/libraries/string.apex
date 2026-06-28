import os
import string

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function string_len()
    len = string.len("Hello")
    assert_equal(len, 5, "string.len Hello")
    
    len = string.len("")
    assert_equal(len, 0, "string.len empty")

function string_lower()
    result = string.lower("HELLO")
    assert_equal(result, "hello", "string.lower HELLO")
    
    result = string.lower("Hello World")
    assert_equal(result, "hello world", "string.lower Hello World")

function string_upper()
    result = string.upper("hello")
    assert_equal(result, "HELLO", "string.upper hello")
    
    result = string.upper("Hello World")
    assert_equal(result, "HELLO WORLD", "string.upper Hello World")

function string_sub()
    result = string.sub("Hello World", 0, 5)
    assert_equal(result, "Hello", "string.sub 0-5")
    
    result = string.sub("Hello World", 6, 11)
    assert_equal(result, "World", "string.sub 6-11")

function string_split()
    parts = string.split("a,b,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split[1]")
        assert_equal(parts[2], "b", "string.split[2]")
        assert_equal(parts[3], "c", "string.split[3]")

function string_join()
    parts = string.split("a,b,c", ",")
    result = string.join(parts, "-")
    assert_equal(result, "a-b-c", "string.join with dash")
    
    result = string.join(parts, "")
    assert_equal(result, "abc", "string.join empty separator")

function string_trim()
    result = string.trim("  hello  ")
    assert_equal(result, "hello", "string.trim spaces")
    
    result = string.trim("\t\nhello\t\n")
    assert_equal(result, "hello", "string.trim tabs newlines")
    
    result = string.trim("hello")
    assert_equal(result, "hello", "string.trim no change")

function string_find()
    index = string.find("Hello World", "World")
    assert_equal(index, 6, "string.find World")
    
    index = string.find("Hello World", "Hello")
    assert_equal(index, 0, "string.find Hello")
    
    index = string.find("Hello World", "xyz")
    assert_equal(index, -1, "string.find not found")

function string_replace()
    result = string.replace("Hello World", "World", "Apex")
    assert_equal(result, "Hello Apex", "string.replace World->Apex")
    
    result = string.replace("Hello World", "xyz", "abc")
    assert_equal(result, "Hello World", "string.replace not found")

string_len()
string_lower()
string_upper()
string_sub()
string_split()
string_join()
string_trim()
string_find()
string_replace()

if tests_failed == 0
    os.output("[OK] libraries/string.apex")