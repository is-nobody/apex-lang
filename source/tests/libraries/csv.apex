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
    quoted_csv = "name,description\r\n\"Smith, John\",\"Engineer\""
    result = csv.decode(quoted_csv)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "csv_decode handles quoted fields")
    if is_table == true
        assert_equal(result[1]["name"], "Smith, John", "csv_decode quoted name with comma")
        assert_equal(result[1]["description"], "Engineer", "csv_decode quoted description")

// test: csv_encode and csv_decode with headers
function csv_encode_read_headers()
    data = [
        ["id" = 1, "name" = "Alice"],
        ["id" = 2, "name" = "Bob"]
    ]
    csv_string = csv.encode(data)
    assert_equal(type(csv_string), "string", "csv_encode with headers")
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_decode with headers returns table")
    if type(read_data) == "table"
        assert_equal(read_data[1]["id"], 1, "csv_decode header row 1 id")
        assert_equal(read_data[2]["id"], 2, "csv_decode header row 2 id")

// test: csv_encode and csv_decode with quoted fields containing newlines
function csv_multiline_fields()
    data = [
        ["id" = 1, "notes" = "line1\nline2"],
        ["id" = 2, "notes" = "simple"]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_decode multiline fields")
    if type(read_data) == "table"
        assert_equal(read_data[1]["notes"], "line1\nline2", "csv_decode multiline field value")

// test: csv_encode handles empty table
function csv_encode_empty_table()
    data = []
    csv_string = csv.encode(data)
    assert_equal(type(csv_string), "string", "csv_encode empty table returns string")
    assert_equal(csv_string, "", "csv_encode empty table returns empty string")

// test: csv_decode handles single row without trailing newline
function csv_decode_single_row_no_newline()
    csv_data = "name,age\nAlice,30"
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode single row no newline")
    if type(result) == "table"
        assert_equal(result[1]["name"], "Alice", "csv_decode single row name")
        assert_equal(result[1]["age"], 30, "csv_decode single row age")

// test: csv_encode preserves numeric types on decode
function csv_numeric_types()
    data = [
        ["int" = 42, "float" = 3.14, "negative" = -17, "zero" = 0]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_decode numeric types")
    if type(read_data) == "table"
        assert_equal(read_data[1]["int"], 42, "csv_decode integer value")
        assert_equal(read_data[1]["float"], 3.14, "csv_decode float value")
        assert_equal(read_data[1]["negative"], -17, "csv_decode negative value")
        assert_equal(read_data[1]["zero"], 0, "csv_decode zero value")

// test: csv_encode preserves boolean types on decode
function csv_boolean_types()
    data = [
        ["true_val" = true, "false_val" = false]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_decode boolean types")
    if type(read_data) == "table"
        assert_equal(read_data[1]["true_val"], true, "csv_decode true value")
        assert_equal(read_data[1]["false_val"], false, "csv_decode false value")

// test: csv_decode handles quoted fields with escaped quotes
function csv_escaped_quotes()
    csv_data = "name,quote\n\"John \"\"Johnny\"\" Doe\",\"He said \"\"hi\"\"\""
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode escaped quotes")
    if type(result) == "table"
        assert_equal(result[1]["name"], "John \"Johnny\" Doe", "csv_decode escaped quotes in name")
        assert_equal(result[1]["quote"], "He said \"hi\"", "csv_decode escaped quotes in quote")

// test: csv_encode properly escapes quotes
function csv_encode_escaped_quotes()
    data = [
        ["name" = "John \"Johnny\" Doe", "quote" = "He said \"hi\""]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_encode escaped quotes")
    if type(read_data) == "table"
        assert_equal(read_data[1]["name"], "John \"Johnny\" Doe", "csv_encode escape quotes name")
        assert_equal(read_data[1]["quote"], "He said \"hi\"", "csv_encode escape quotes quote")

// test: csv_decode handles leading and trailing spaces
function csv_spaces()
    csv_data = "name,age\n  Alice  ,  30  "
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode spaces")
    if type(result) == "table"
        assert_equal(result[1]["name"], "  Alice  ", "csv_decode preserves spaces in name")
        assert_equal(result[1]["age"], "  30  ", "csv_decode preserves spaces as string")

// test: csv_decode handles multiple consecutive delimiters (empty fields)
function csv_empty_fields()
    csv_data = "col1,col2,col3\n,,\nval1,,val3"
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode empty fields")
    if type(result) == "table"
        assert_equal(result[1]["col1"], "", "csv_decode empty col1")
        assert_equal(result[1]["col2"], "", "csv_decode empty col2")
        assert_equal(result[1]["col3"], "", "csv_decode empty col3")
        assert_equal(result[2]["col1"], "val1", "csv_decode val1")
        assert_equal(result[2]["col2"], "", "csv_decode empty middle field")
        assert_equal(result[2]["col3"], "val3", "csv_decode val3")

// test: csv_encode handles missing keys in rows
function csv_missing_keys()
    data = [
        ["id" = 1, "name" = "Alice", "extra" = "x"],
        ["id" = 2, "name" = "Bob"]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv_decode missing keys")
    if type(read_data) == "table"
        assert_equal(read_data[1]["id"], 1, "csv_decode missing keys id 1")
        assert_equal(read_data[1]["name"], "Alice", "csv_decode missing keys name 1")
        assert_equal(read_data[2]["id"], 2, "csv_decode missing keys id 2")
        assert_equal(read_data[2]["name"], "Bob", "csv_decode missing keys name 2")

// test: csv_decode handles crlf line endings
function csv_crlf_endings()
    csv_data = "name,age\r\nAlice,30\r\nBob,25\r\n"
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode crlf endings")
    if type(result) == "table"
        assert_equal(result[1]["name"], "Alice", "csv_decode crlf row 1 name")
        assert_equal(result[2]["name"], "Bob", "csv_decode crlf row 2 name")

// test: csv_decode handles lf line endings
function csv_lf_endings()
    csv_data = "name,age\nAlice,30\nBob,25\n"
    result = csv.decode(csv_data)
    assert_equal(type(result), "table", "csv_decode lf endings")
    if type(result) == "table"
        assert_equal(result[1]["name"], "Alice", "csv_decode lf row 1 name")
        assert_equal(result[2]["name"], "Bob", "csv_decode lf row 2 name")

// test: csv roundtrip with complex data
function csv_roundtrip_complex()
    data = [
        ["id" = 1, "name" = "Smith, John", "bio" = "Line 1\nLine 2", "quote" = "He said \"hello\"", "active" = true, "score" = 98.6],
        ["id" = 2, "name" = "Doe, Jane", "bio" = "Simple bio", "quote" = "", "active" = false, "score" = -1.5]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv roundtrip complex")
    if type(read_data) == "table"
        assert_equal(read_data[1]["id"], 1, "csv roundtrip id 1")
        assert_equal(read_data[1]["name"], "Smith, John", "csv roundtrip name 1")
        assert_equal(read_data[1]["bio"], "Line 1\nLine 2", "csv roundtrip bio 1")
        assert_equal(read_data[1]["quote"], "He said \"hello\"", "csv roundtrip quote 1")
        assert_equal(read_data[1]["active"], true, "csv roundtrip active 1")
        assert_equal(read_data[1]["score"], 98.6, "csv roundtrip score 1")
        assert_equal(read_data[2]["id"], 2, "csv roundtrip id 2")
        assert_equal(read_data[2]["name"], "Doe, Jane", "csv roundtrip name 2")
        assert_equal(read_data[2]["bio"], "Simple bio", "csv roundtrip bio 2")
        assert_equal(read_data[2]["quote"], "", "csv roundtrip quote 2")
        assert_equal(read_data[2]["active"], false, "csv roundtrip active 2")
        assert_equal(read_data[2]["score"], -1.5, "csv roundtrip score 2")

// test: csv_encode with string keys
function csv_string_keys()
    data = [
        ["1" = "one", "2" = "two"],
        ["1" = "three", "2" = "four"]
    ]
    csv_string = csv.encode(data)
    read_data = csv.decode(csv_string)
    assert_equal(type(read_data), "table", "csv string keys")
    if type(read_data) == "table"
        assert_equal(read_data[1]["1"], "one", "csv string keys row 1 col 1")
        assert_equal(read_data[1]["2"], "two", "csv string keys row 1 col 2")
        assert_equal(read_data[2]["1"], "three", "csv string keys row 2 col 1")
        assert_equal(read_data[2]["2"], "four", "csv string keys row 2 col 2")

// execute all csv library test cases
csv_encode_read()
csv_invalid_input()
csv_quoted_fields()
csv_encode_read_headers()
csv_multiline_fields()
csv_encode_empty_table()
csv_decode_single_row_no_newline()
csv_numeric_types()
csv_boolean_types()
csv_escaped_quotes()
csv_encode_escaped_quotes()
csv_spaces()
csv_empty_fields()
csv_missing_keys()
csv_crlf_endings()
csv_lf_endings()
csv_roundtrip_complex()
csv_string_keys()

// report completion of csv library tests
os.output("[PASS] libraries/csv.apex")