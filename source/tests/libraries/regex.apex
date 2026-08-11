// source/tests/libraries/regex.apex
// Implementation of Regular Expression Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import table
import regex

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: compares two tables element-wise
function assert_table_equal(actual, expected, name)
    if type(actual) != "table" or type(expected) != "table"
        os.output("[ERROR] {name}: both must be tables")
        return

    if table.size(actual) != table.size(expected)
        os.output("[ERROR] {name}: table sizes differ")
        return

    for i = 1, table.size(actual)
        if actual[i] != expected[i]
            os.output("[ERROR] {name}[{i}]: got {actual[i]}, expected {expected[i]}")
            return

// test: findall extracts all digit sequences
function regex_findall()
    result = regex.find_all("\\d+", "abc 123 def 456")
    assert_equal(result[1], "123", "findall digits [1]")
    assert_equal(result[2], "456", "findall digits [2]")
    
    result = regex.find_all("[a-z]+", "Hello World")
    assert_equal(result[1], "ello", "findall lowercase [1]")
    assert_equal(result[2], "orld", "findall lowercase [2]")
    
    result = regex.find_all("\\w+", "hello world")
    assert_equal(result[1], "hello", "findall words [1]")
    assert_equal(result[2], "world", "findall words [2]")
    
    result = regex.find_all("x", "hello")
    assert_equal(table.size(result), 0, "findall no match")

// test: findall with ignorecase flag
function regex_findall_ignorecase()
    opts = ["ignorecase" = true]
    result = regex.find_all("[a-z]+", "HELLO World", opts)
    assert_equal(result[1], "HELLO", "findall ignorecase [1]")
    assert_equal(result[2], "World", "findall ignorecase [2]")

// test: findall with dotall flag for multiline matching
function regex_findall_dotall()
    opts = ["dotall" = true]
    result = regex.find_all("h.*o", "hello\nworld", opts)
    assert_equal(result[1], "hello\nwo", "findall dotall multiline")
    
    result = regex.find_all("h.*o", "hello\nworld")
    assert_equal(table.size(result), 1, "findall no dotall default")

// test: findall with start and end anchors
function regex_findall_anchors()
    result = regex.find_all("^\\w+", "hello world")
    assert_equal(result[1], "hello", "findall start anchor")
    
    result = regex.find_all("\\w+$", "hello world")
    assert_equal(result[1], "world", "findall end anchor")

// test: findall with character classes
function regex_findall_charclass()
    result = regex.find_all("[aeiou]+", "hello world")
    assert_equal(result[1], "e", "findall vowels [1]")
    assert_equal(result[2], "o", "findall vowels [2]")
    assert_equal(result[3], "o", "findall vowels [3]")
    
    result = regex.find_all("[^aeiou\\s]+", "hello world")
    assert_equal(result[1], "h", "findall not vowels [1]")
    assert_equal(result[2], "ll", "findall not vowels [2]")
    assert_equal(result[3], "w", "findall not vowels [3]")
    assert_equal(result[4], "rld", "findall not vowels [4]")

// test: findall with quantifiers (*, +, ?)
function regex_findall_quantifiers()
    result = regex.find_all("ab*c", "ac abc abbc abbbc")
    assert_equal(result[1], "ac", "findall star zero")
    assert_equal(result[2], "abc", "findall star one")
    assert_equal(result[3], "abbc", "findall star two")
    assert_equal(result[4], "abbbc", "findall star three")
    
    result = regex.find_all("ab+c", "ac abc abbc abbbc")
    assert_equal(result[1], "abc", "findall plus one")
    assert_equal(result[2], "abbc", "findall plus two")
    assert_equal(result[3], "abbbc", "findall plus three")
    
    result = regex.find_all("ab?c", "ac abc abbc")
    assert_equal(result[1], "ac", "findall question zero")
    assert_equal(result[2], "abc", "findall question one")

// test: findall with special character classes (\s, \S, \d, \D, \w, \W)
function regex_findall_special()
    result = regex.find_all("\\s+", "hello\tworld\nfoo bar")
    assert_equal(result[1], "\t", "findall whitespace tab")
    assert_equal(result[2], "\n", "findall whitespace newline")
    assert_equal(result[3], " ", "findall whitespace space")
    
    result = regex.find_all("\\S+", "hello world")
    assert_equal(result[1], "hello", "findall non-whitespace [1]")
    assert_equal(result[2], "world", "findall non-whitespace [2]")
    
    result = regex.find_all("\\D+", "abc123def456")
    assert_equal(result[1], "abc", "findall non-digits [1]")
    assert_equal(result[2], "def", "findall non-digits [2]")
    
    result = regex.find_all("\\W+", "hello_world foo-bar")
    assert_equal(result[1], " ", "findall non-word space")
    assert_equal(result[2], "-", "findall non-word dash")

// test: findall with invalid input returns none
function regex_findall_invalid()
    n = none
    num = 123
    
    function test_findall(pattern, text)
        return regex.find_all(pattern, text)
    
    assert_equal(test_findall(num, "hello"), n, "findall invalid pattern")
    assert_equal(test_findall(n, "hello"), n, "findall none pattern")
    assert_equal(test_findall("\\d+", num), n, "findall invalid text")
    assert_equal(test_findall("\\d+", n), n, "findall none text")

// test: sub replaces matched patterns with replacement
function regex_sub()
    result = regex.replace("\\d+", "NUM", "abc 123 def 456")
    assert_equal(result, "abc NUM def NUM", "sub digits to NUM")
    
    result = regex.replace("\\s+", "-", "hello world foo")
    assert_equal(result, "hello-world-foo", "sub whitespace to dash")
    
    result = regex.replace("x", "y", "hello")
    assert_equal(result, "hello", "sub no match")
    
    result = regex.replace("a", "b", "aaa")
    assert_equal(result, "bbb", "sub multiple matches")

// test: sub with ignorecase flag
function regex_sub_ignorecase()
    opts = ["ignorecase" = true]
    result = regex.replace("[a-z]+", "WORD", "Hello World", opts)
    assert_equal(result, "WORD WORD", "sub ignorecase")

// test: sub with invalid input returns none
function regex_sub_invalid()
    n = none
    num = 123
    
    function test_sub(pattern, repl, text)
        return regex.replace(pattern, repl, text)
    
    assert_equal(test_sub("\\d", "X", n), n, "sub none text")
    assert_equal(test_sub("\\d", n, "hello"), n, "sub none replacement")
    assert_equal(test_sub(n, "X", "hello"), n, "sub none pattern")
    assert_equal(test_sub(num, "X", "hello"), n, "sub number pattern")

// test: split divides string by pattern
function regex_split()
    parts = regex.split(",\\s*", "a, b, c, d")
    assert_equal(parts[1], "a", "split comma+space [1]")
    assert_equal(parts[2], "b", "split comma+space [2]")
    assert_equal(parts[3], "c", "split comma+space [3]")
    assert_equal(parts[4], "d", "split comma+space [4]")
    
    parts = regex.split("\\d+", "abc123def456ghi")
    assert_equal(parts[1], "abc", "split digits [1]")
    assert_equal(parts[2], "def", "split digits [2]")
    assert_equal(parts[3], "ghi", "split digits [3]")
    
    parts = regex.split("\\s+", "hello world foo bar")
    assert_equal(parts[1], "hello", "split whitespace [1]")
    assert_equal(parts[2], "world", "split whitespace [2]")
    assert_equal(parts[3], "foo", "split whitespace [3]")
    assert_equal(parts[4], "bar", "split whitespace [4]")
    
    parts = regex.split("x", "hello")
    assert_equal(table.size(parts), 1, "split no delimiter")
    assert_equal(parts[1], "hello", "split no delimiter [1]")
    
    parts = regex.split("\\s+", "   hello   world   ")
    assert_equal(parts[1], "hello", "split leading trailing spaces [1]")
    assert_equal(parts[2], "world", "split leading trailing spaces [2]")

// test: split with ignorecase flag
function regex_split_ignorecase()
    opts = ["ignorecase" = true]
    parts = regex.split("[a-z]+", "123ABC456def789", opts)
    assert_equal(parts[1], "123", "split ignorecase [1]")
    assert_equal(parts[2], "456", "split ignorecase [2]")
    assert_equal(parts[3], "789", "split ignorecase [3]")

// test: split with invalid input returns none
function regex_split_invalid()
    n = none
    num = 123
    
    function test_split(pattern, text)
        return regex.split(pattern, text)
    
    assert_equal(test_split("\\d", n), n, "split none text")
    assert_equal(test_split(n, "hello"), n, "split none pattern")
    assert_equal(test_split(num, "hello"), n, "split number pattern")

// test: search finds first match and returns metadata
function regex_search()
    result = regex.search("\\d+", "abc 123 def 456")
    assert_equal(result["start"], 5, "search start pos")
    assert_equal(result["end"], 7, "search end pos")
    assert_equal(result["match"], "123", "search match first")
    
    result = regex.search("[a-z]+", "123 HELLO world")
    assert_equal(result["start"], 11, "search lowercase start")
    assert_equal(result["end"], 15, "search lowercase end")
    assert_equal(result["match"], "world", "search lowercase match")
    
    result = regex.search("^\\w+", "hello world")
    assert_equal(result["start"], 1, "search anchor start pos")
    assert_equal(result["end"], 5, "search anchor end pos")
    assert_equal(result["match"], "hello", "search anchor match")
    
    result = regex.search("\\w+$", "hello world")
    assert_equal(result["start"], 7, "search anchor end pos")
    assert_equal(result["end"], 11, "search anchor end end")
    assert_equal(result["match"], "world", "search anchor end match")
    
    result = regex.search("xyz", "hello world")
    assert_equal(table.size(result), 0, "search no match empty table")

// test: search with ignorecase flag
function regex_search_ignorecase()
    opts = ["ignorecase" = true]
    result = regex.search("[a-z]+", "123 HELLO", opts)
    assert_equal(result["start"], 5, "search ignorecase start")
    assert_equal(result["end"], 9, "search ignorecase end")
    assert_equal(result["match"], "HELLO", "search ignorecase match")

// test: search with invalid input returns none
function regex_search_invalid()
    n = none
    num = 123
    
    function test_search(pattern, text)
        return regex.search(pattern, text)
    
    assert_equal(test_search("\\d", n), n, "search none text")
    assert_equal(test_search(n, "hello"), n, "search none pattern")
    assert_equal(test_search(num, "hello"), n, "search number pattern")

// test: escape sequences for literal backslash patterns
function regex_escape_sequences()
    result = regex.find_all("\\\\d", "abc\\d123")
    assert_equal(result[1], "\\d", "findall literal backslash-d")
    
    result = regex.find_all("\\\\w", "abc\\w123")
    assert_equal(result[1], "\\w", "findall literal backslash-w")
    
    result = regex.find_all("\\\\s", "abc\\s123")
    assert_equal(result[1], "\\s", "findall literal backslash-s")
    
    result = regex.find_all("\\[test\\]", "[test] hello [test]")
    assert_equal(result[1], "[test]", "findall literal brackets [1]")
    assert_equal(result[2], "[test]", "findall literal brackets [2]")

// execute all regex library test cases
regex_findall()
regex_findall_ignorecase()
regex_findall_dotall()
regex_findall_anchors()
regex_findall_charclass()
regex_findall_quantifiers()
regex_findall_special()
regex_findall_invalid()
regex_sub()
regex_sub_ignorecase()
regex_sub_invalid()
regex_split()
regex_split_ignorecase()
regex_split_invalid()
regex_search()
regex_search_ignorecase()
regex_search_invalid()
regex_escape_sequences()

// report completion of regex library tests
os.output("[PASS] libraries/regex.apex")