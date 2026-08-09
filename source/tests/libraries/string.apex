// source/tests/libraries/string.apex
// Implementation of String Library Function Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: string length returns character count
function string_len()
    len = string.length("Hello")
    assert_equal(len, 5, "string.length Hello")
    
    len = string.length("")
    assert_equal(len, 0, "string.length empty")

// test: convert string to lowercase
function string_lower()
    result = string.lower("HELLO")
    assert_equal(result, "hello", "string.lower HELLO")
    
    result = string.lower("Hello World")
    assert_equal(result, "hello world", "string.lower Hello World")

// test: convert string to uppercase
function string_upper()
    result = string.upper("hello")
    assert_equal(result, "HELLO", "string.upper hello")
    
    result = string.upper("Hello World")
    assert_equal(result, "HELLO WORLD", "string.upper Hello World")

// test: substring extraction with various scripts and edge cases
function string_sub()
    result = string.slice("Hello World", 1, 5)
    assert_equal(result, "Hello", "string.slice english 1-5")
    
    result = string.slice("Hello World", 7, 11)
    assert_equal(result, "World", "string.slice english 7-11")
    
    result = string.slice("你好世界", 1, 2)
    assert_equal(result, "你好", "string.slice chinese 1-2")
    
    result = string.slice("你好世界", 3, 4)
    assert_equal(result, "世界", "string.slice chinese 3-4")
    
    result = string.slice("Привет Мир", 1, 6)
    assert_equal(result, "Привет", "string.slice russian 1-6")
    
    result = string.slice("Привет Мир", 8, 10)
    assert_equal(result, "Мир", "string.slice russian 8-10")
    
    result = string.slice("السلام عليكم", 1, 6)
    assert_equal(result, "السلام", "string.slice arabic 1-6")
    
    result = string.slice("السلام عليكم", 8, 13)
    assert_equal(result, "عليكم", "string.slice arabic 8-13")
    
    result = string.slice("Hello Привет 你好", 1, 5)
    assert_equal(result, "Hello", "string.slice mixed 1-5")
    
    result = string.slice("Hello Привет 你好", 7, 12)
    assert_equal(result, "Привет", "string.slice mixed 7-12")
    
    result = string.slice("Hello Привет 你好", 14, 15)
    assert_equal(result, "你好", "string.slice mixed 14-15")
    
    result = string.slice("", 1, 5)
    assert_equal(result, "", "string.slice empty")
    
    result = string.slice("Hello", 1, 10)
    assert_equal(result, "Hello", "string.slice beyond end")
    
    result = string.slice("Hello", -4, 5)
    assert_equal(result, "Hello", "string.slice negative start")
    
    result = string.slice("😊🌟", 1, 1)
    assert_equal(result, "😊", "string.slice emoji 1-1")
    
    result = string.slice("😊🌟", 2, 2)
    assert_equal(result, "🌟", "string.slice emoji 2-2")

// test: split string by delimiter into table
function string_split()
    parts = string.split("a,b,c", ",")
    is_table = type(parts) == "table"
    assert_equal(is_table, true, "string.split returns table")
    if is_table == true
        assert_equal(parts[1], "a", "string.split[1]")
        assert_equal(parts[2], "b", "string.split[2]")
        assert_equal(parts[3], "c", "string.split[3]")

// test: join table elements into string with separator
function string_join()
    parts = string.split("a,b,c", ",")
    result = string.join(parts, "-")
    assert_equal(result, "a-b-c", "string.join with dash")
    
    result = string.join(parts, "")
    assert_equal(result, "abc", "string.join empty separator")

// test: trim whitespace from both ends
function string_trim()
    result = string.trim("  hello  ")
    assert_equal(result, "hello", "string.trim spaces")
    
    result = string.trim("\t\nhello\t\n")
    assert_equal(result, "hello", "string.trim tabs newlines")
    
    result = string.trim("hello")
    assert_equal(result, "hello", "string.trim no change")

// test: find substring position
function string_find()
    index = string.find("Hello World", "World")
    assert_equal(index, 7, "string.find World")
    
    index = string.find("Hello World", "Hello")
    assert_equal(index, 1, "string.find Hello")
    
    index = string.find("Hello World", "xyz")
    assert_equal(index, -1, "string.find not found")

// test: replace substring with another
function string_replace()
    result = string.replace("Hello World", "World", "Apex")
    assert_equal(result, "Hello Apex", "string.replace World->Apex")
    
    result = string.replace("Hello World", "xyz", "abc")
    assert_equal(result, "Hello World", "string.replace not found")

// test: all string functions return none for invalid input types
function string_all_none_checks()
    num = 123
    bool_val = true
    nil_val = none
    
    function test_len(val)
        result = string.length(val)
        return result
    
    assert_equal(test_len(num), none, "len(number)")
    assert_equal(test_len(bool_val), none, "len(bool)")
    assert_equal(test_len(nil_val), none, "len(none)")
    
    function test_upper(val)
        return string.upper(val)
    
    assert_equal(test_upper(num), none, "upper(number)")
    assert_equal(test_upper(bool_val), none, "upper(bool)")
    assert_equal(test_upper(nil_val), none, "upper(none)")
    
    function test_lower(val)
        return string.lower(val)
    
    assert_equal(test_lower(num), none, "lower(number)")
    assert_equal(test_lower(bool_val), none, "lower(bool)")
    assert_equal(test_lower(nil_val), none, "lower(none)")
    
    function test_trim(val)
        return string.trim(val)
    
    assert_equal(test_trim(num), none, "trim(number)")
    assert_equal(test_trim(bool_val), none, "trim(bool)")
    assert_equal(test_trim(nil_val), none, "trim(none)")
    
    function test_find(a, b)
        return string.find(a, b)
    
    assert_equal(test_find(num, "test"), none, "find(num,str)")
    assert_equal(test_find("test", num), none, "find(str,num)")
    assert_equal(test_find(bool_val, "test"), none, "find(bool,str)")
    assert_equal(test_find("test", bool_val), none, "find(str,bool)")
    assert_equal(test_find(nil_val, "test"), none, "find(none,str)")
    assert_equal(test_find("test", nil_val), none, "find(str,none)")
    
    function test_replace(a, b, c)
        return string.replace(a, b, c)
    
    assert_equal(test_replace(num, "a", "b"), none, "replace(num,str,str)")
    assert_equal(test_replace("test", num, "b"), none, "replace(str,num,str)")
    assert_equal(test_replace("test", "a", num), none, "replace(str,str,num)")
    assert_equal(test_replace(nil_val, "a", "b"), none, "replace(none,str,str)")
    assert_equal(test_replace("test", nil_val, "b"), none, "replace(str,none,str)")
    assert_equal(test_replace("test", "a", nil_val), none, "replace(str,str,none)")
    
    function test_sub(a, b, c)
        return string.slice(a, b, c)
    
    assert_equal(test_sub(num, 0, 5), none, "sub(num,int,int)")
    assert_equal(test_sub("test", "0", 5), none, "sub(str,str,int)")
    assert_equal(test_sub("test", 0, "5"), none, "sub(str,int,str)")
    assert_equal(test_sub(nil_val, 0, 5), none, "sub(none,int,int)")
    assert_equal(test_sub("test", nil_val, 5), none, "sub(str,none,int)")
    assert_equal(test_sub("test", 0, nil_val), none, "sub(str,int,none)")
    
    function test_split(val)
        return string.split(val, ",")
    
    assert_equal(test_split(num), none, "split(number)")
    assert_equal(test_split(bool_val), none, "split(bool)")
    assert_equal(test_split(nil_val), none, "split(none)")
    
    function test_join(val, sep)
        return string.join(val, sep)
    
    assert_equal(test_join(num, ","), none, "join(number,str)")
    assert_equal(test_join(bool_val, ","), none, "join(bool,str)")
    assert_equal(test_join(nil_val, ","), none, "join(none,str)")
    assert_equal(test_join("not table", ","), none, "join(string,str)")

// execute all string library test cases
string_len()
string_lower()
string_upper()
string_sub()
string_split()
string_join()
string_trim()
string_find()
string_replace()
string_all_none_checks()

// report completion of string library tests
os.output("[PASS] libraries/string.apex")