// source/tests/libraries/csv.apex
// Implementation of CSV Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import csv
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: csv write and read with structured data
function csv_encode_read()
    data = [
        ["id" = 1, "name" = "Alice", "age" = 30, "active" = true],
        ["id" = 2, "name" = "Bob, Jr.", "age" = 25, "active" = false],
        ["id" = 3, "name" = "Charlie", "age" = 40, "active" = true]
    ]
    
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    
    is_table = type(read_data) == "table"
    assert_equal(is_table, true, "csv_decode returns table")
    if is_table == true
        assert_equal(read_data[1]["name"], "Alice", "csv row 1 name")
        assert_equal(read_data[2]["name"], "Bob, Jr.", "csv row 2 name")
        assert_equal(read_data[3]["name"], "Charlie", "csv row 3 name")

// test: csv_decode handles empty input gracefully
function csv_invalid_input()
    empty_csv = ""
    result = csv.decode(empty_csv)
    assert_equal(type(result), "none", "csv_decode empty input returns none")

// test: csv_decode handles quoted fields with embedded commas
function csv_quoted_fields()
    quoted_csv = "name,description\n\"Smith, John\",\"Engineer\""
    result = csv.decode(quoted_csv)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "csv_decode handles quoted fields")
    if is_table == true
        assert_equal(result[1]["name"], "Smith, John", "csv_decode quoted name with comma")
        assert_equal(result[1]["description"], "Engineer", "csv_decode quoted description")

// test: csv_encode and csv_decode with custom delimiter
function csv_encode_custom_delimiter()
    data = [
        ["id" = 1, "name" = "Alice"],
        ["id" = 2, "name" = "Bob"]
    ]
    csv_string = csv.encode(data, true, ";")
    assert_equal(type(csv_string), "string", "csv_encode custom delimiter")
    read_data = csv.decode(csv_string, true, ";")
    assert_equal(read_data[1]["id"], 1, "csv_decode custom delimiter")

// execute all csv library test cases
csv_encode_read()
csv_invalid_input()
csv_quoted_fields()
csv_encode_custom_delimiter()

// report completion of csv library tests
os.output("[PASS] libraries/csv.apex")