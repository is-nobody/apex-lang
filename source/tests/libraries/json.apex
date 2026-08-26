// source/tests/libraries/json.apex
// Implementation of JSON Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import json
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: json serialization of primitive types
function json_primitives()
    assert_equal(json.encode(42), "42", "json_encode number")
    assert_equal(json.encode("hello"), "\"hello\"", "json_encode string")
    assert_equal(json.encode(true), "true", "json_encode true")
    assert_equal(json.encode(none), "null", "json_encode none")

// test: json serialization of arrays
function json_array()
    arr = ["apple", "banana", "cherry"]
    json_arr = json.encode(arr)
    assert_equal(json_arr, "[\"apple\", \"banana\", \"cherry\"]", "json_encode array")

// test: json object roundtrip (write then read)
function json_object()
    obj = ["name" = "Apex", "version" = 1.0, "is_cool" = true]
    json_obj = json.encode(obj)
    readd = json.decode(json_obj)
    assert_equal(readd["name"], "Apex", "json object roundtrip name")
    assert_equal(readd["is_cool"], true, "json object roundtrip is_cool")

// test: json read parses complex nested structure
function json_decode()
    original = ["status" = "ok", "codes" = [200, 404]]
    raw_json = json.encode(original)
    data = json.decode(raw_json)
    is_table = type(data) == "table"
    assert_equal(is_table, true, "json_decode returns table")
    if is_table == true
        assert_equal(data["status"], "ok", "json read status")
        assert_equal(data["codes"][1], 200, "json read first code")

// test: json_decode handles invalid json gracefully
function json_invalid_input()
    invalid_json = "\{name: \"test\"\}"
    result = json.decode(invalid_json)
    assert_equal(type(result), "none", "json_decode invalid input returns none")
    
    invalid_json2 = "\{\"name\": \"test\",\}"
    result2 = json.decode(invalid_json2)
    assert_equal(type(result2), "none", "json_decode trailing comma returns none")

// test: json_decode handles escaped characters and unicode
function json_escape_handling()
    escaped_json = "\{\"text\": \"Hello\\nWorld\\tTest\"\}"
    result = json.decode(escaped_json)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "json_decode handles escaped characters")
    if is_table == true
        assert_equal(result["text"], "Hello\nWorld\tTest", "json_decode correctly unescapes")
    
    unicode_json = "\{\"emoji\": \"\\u2764\"\}"
    result2 = json.decode(unicode_json)
    is_table2 = type(result2) == "table"
    assert_equal(is_table2, true, "json_decode handles unicode escapes")

// test: json_encode escapes special characters properly
function json_encode_escaped_strings()
    special_str = "Hello\n\"World\"\tTab"
    json_str = json.encode(special_str)
    assert_equal(type(json_str), "string", "json_encode escapes special characters")
    
    parsed = json.decode(json_str)
    assert_equal(parsed, special_str, "json_encode roundtrip escaped string")

// execute all json library test cases
json_primitives()
json_array()
json_object()
json_decode()
json_invalid_input()
json_escape_handling()
json_encode_escaped_strings()

// report completion of json library tests
os.output("[PASS] libraries/json.apex")