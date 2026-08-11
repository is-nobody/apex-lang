// source/tests/libraries/codecs.apex
// Implementation of Codecs Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import codecs
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: base64 encoding and decoding roundtrip
function base64_roundtrip()
    text = "Hello, World! This is a test of Base64."
    b64_str = codecs.base_encode(text)
    decoded_b64 = codecs.base_decode(b64_str)
    assert_equal(decoded_b64, text, "base64 roundtrip")

// test: base64url encoding and decoding roundtrip
function base64url_roundtrip()
    url_data = "data?param=value&other=123"
    b64url_str = codecs.baseurl_encode(url_data)
    decoded_b64url = codecs.baseurl_decode(b64url_str)
    assert_equal(decoded_b64url, url_data, "base64url roundtrip")

// test: hex encoding and decoding roundtrip
function hex_roundtrip()
    input_text = "Hello, World!"
    hex_result = codecs.hex_encode(input_text)
    decoded_result = codecs.hex_decode(hex_result)
    assert_equal(decoded_result, input_text, "hex roundtrip")
    assert_equal(hex_result, "48656c6c6f2c20576f726c6421", "hex write lowercase output")

// test: hex encoding handles UTF-8 text
function hex_utf8_text()
    utf8_input = "Hello 世界"
    hex_result = codecs.hex_encode(utf8_input)
    decoded_result = codecs.hex_decode(hex_result)
    assert_equal(decoded_result, utf8_input, "hex UTF-8 roundtrip")

// test: hex encoding handles binary data
function hex_binary_data()
    binary_data = codecs.hex_decode("00010203FFFEFD")
    hex_result = codecs.hex_encode(binary_data)
    decoded_result = codecs.hex_decode(hex_result)
    assert_equal(decoded_result, binary_data, "hex binary data roundtrip")
    assert_equal(hex_result, "00010203fffefd", "hex binary encoding")

// test: hex read is case insensitive
function hex_case_insensitive()
    lower_input = "48656c6c6f"
    upper_input = "48656C6C6F"
    mixed_input = "48656c6C6f"
    
    decoded_lower = codecs.hex_decode(lower_input)
    decoded_upper = codecs.hex_decode(upper_input)
    decoded_mixed = codecs.hex_decode(mixed_input)
    
    assert_equal(decoded_lower, "Hello", "hex lowercase decode")
    assert_equal(decoded_upper, "Hello", "hex uppercase decode")
    assert_equal(decoded_mixed, "Hello", "hex mixed case decode")

// test: hex read handles spaces and formatting
function hex_with_spaces()
    spaced_hex = "48 65 6c 6c 6f"
    newline_hex = "48\n65\t6c\r6c6f"
    
    decoded_spaces = codecs.hex_decode(spaced_hex)
    decoded_newlines = codecs.hex_decode(newline_hex)
    
    assert_equal(decoded_spaces, "Hello", "hex decode with spaces")
    assert_equal(decoded_newlines, "Hello", "hex decode with whitespace")

// test: hex read handles odd number of hex characters
function hex_odd_characters()
    odd_input = "48656c6c6f2"  // missing last nibble
    result = codecs.hex_decode(odd_input)
    assert_equal(type(result), "string", "hex read odd chars returns string")
    
    single_input = "4"
    result2 = codecs.hex_decode(single_input)
    assert_equal(type(result2), "string", "hex read single char returns string")

// test: hex read handles empty input
function hex_empty_input()
    empty_str = ""
    result = codecs.hex_decode(empty_str)
    assert_equal(result, "", "hex read empty string returns empty")
    
    whitespace_only = "   \t\n  "
    result2 = codecs.hex_decode(whitespace_only)
    assert_equal(result2, "", "hex read only whitespace returns empty")

// test: hex read handles invalid characters
function hex_invalid_characters()
    invalid_input = "48GG65"  // G is invalid hex
    result = codecs.hex_decode(invalid_input)
    assert_equal(type(result), "string", "hex read invalid chars returns string")
    assert_equal(result, "He", "hex read skips invalid chars")

// test: csv write and read with structured data
function csv_encode_read()
    data = [
        ["id" = 1, "name" = "Alice", "age" = 30, "active" = true],
        ["id" = 2, "name" = "Bob, Jr.", "age" = 25, "active" = false],
        ["id" = 3, "name" = "Charlie", "age" = 40, "active" = true]
    ]
    
    csv_string = codecs.csv_encode(data)
    read_data = codecs.csv_decode(csv_string)
    
    is_table = type(read_data) == "table"
    assert_equal(is_table, true, "csv_decode returns table")
    if is_table == true
        assert_equal(read_data[1]["name"], "Alice", "csv row 1 name")
        assert_equal(read_data[2]["name"], "Bob, Jr.", "csv row 2 name")
        assert_equal(read_data[3]["name"], "Charlie", "csv row 3 name")

// test: json serialization of primitive types
function json_primitives()
    assert_equal(codecs.json_encode(42), "42", "json_encode number")
    assert_equal(codecs.json_encode("hello"), "\"hello\"", "json_encode string")
    assert_equal(codecs.json_encode(true), "true", "json_encode true")
    assert_equal(codecs.json_encode(none), "null", "json_encode none")

// test: json serialization of arrays
function json_array()
    arr = ["apple", "banana", "cherry"]
    json_arr = codecs.json_encode(arr)
    assert_equal(json_arr, "[\"apple\", \"banana\", \"cherry\"]", "json_encode array")

// test: json object roundtrip (write then read)
function json_object()
    obj = ["name" = "Apex", "version" = 1.0, "is_cool" = true]
    json_obj = codecs.json_encode(obj)
    readd = codecs.json_decode(json_obj)
    assert_equal(readd["name"], "Apex", "json object roundtrip name")
    assert_equal(readd["is_cool"], true, "json object roundtrip is_cool")

// test: json read parses complex nested structure
function json_decode()
    original = ["status" = "ok", "codes" = [200, 404]]
    raw_json = codecs.json_encode(original)
    data = codecs.json_decode(raw_json)
    is_table = type(data) == "table"
    assert_equal(is_table, true, "json_decode returns table")
    if is_table == true
        assert_equal(data["status"], "ok", "json read status")
        assert_equal(data["codes"][1], 200, "json read first code")

// test: xml read parses basic xml structure
function xml_decode()
    simple_xml = "<root><item id=\"1\">Apple</item><item id=\"2\">Banana</item></root>"
    data_xml = codecs.xml_decode(simple_xml)
    is_table = type(data_xml) == "table"
    assert_equal(is_table, true, "xml_decode returns table")
    if is_table == true
        assert_equal(data_xml["__tag"], "root", "xml root tag")
        assert_equal(data_xml[1]["#text"], "Apple", "xml first item text")
        assert_equal(data_xml[1]["@id"], "1", "xml first item id")

// test: xml write serializes table to xml string
function xml_encode()
    xml_obj = [
        "__tag" = "config",
        "@version" = "1.0",
        1 = [
            "__tag" = "setting",
            "@name" = "theme",
            "#text" = "dark"
        ],
        2 = [
            "__tag" = "setting",
            "@name" = "lang",
            "#text" = "en"
        ]
    ]
    writed_xml = codecs.xml_encode(xml_obj)
    is_string = type(writed_xml) == "string"
    assert_equal(is_string, true, "xml_encode returns string")

// test: json_decode handles invalid json gracefully
function json_invalid_input()
    invalid_json = "\{name: \"test\"\}"
    result = codecs.json_decode(invalid_json)
    assert_equal(type(result), "none", "json_decode invalid input returns none")
    
    invalid_json2 = "\{\"name\": \"test\",\}"
    result2 = codecs.json_decode(invalid_json2)
    assert_equal(type(result2), "none", "json_decode trailing comma returns none")

// test: base_decode rejects invalid base64 characters
function base64_invalid_input()
    invalid_b64 = "ABC!DEF"
    result = codecs.base_decode(invalid_b64)
    assert_equal(type(result), "none", "base_decode invalid char returns none")
    
    invalid_b64_2 = "ABCD="
    result2 = codecs.base_decode(invalid_b64_2)
    assert_equal(type(result2), "none", "base_decode wrong padding returns none")

// test: baseurl_decode rejects invalid base64url characters
function base64url_invalid_input()
    invalid_b64url = "ABC!DEF"
    result = codecs.baseurl_decode(invalid_b64url)
    assert_equal(type(result), "none", "baseurl_decode invalid char returns none")

// test: csv_decode handles empty input gracefully
function csv_invalid_input()
    empty_csv = ""
    result = codecs.csv_decode(empty_csv)
    assert_equal(type(result), "none", "csv_decode empty input returns none")

// test: xml_decode handles malformed xml gracefully
function xml_invalid_input()
    invalid_xml = "<root><item>Test</root>"
    result = codecs.xml_decode(invalid_xml)
    assert_equal(type(result), "none", "xml_decode invalid XML returns none")

// test: json_decode handles escaped characters and unicode
function json_escape_handling()
    escaped_json = "\{\"text\": \"Hello\\nWorld\\tTest\"\}"
    result = codecs.json_decode(escaped_json)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "json_decode handles escaped characters")
    if is_table == true
        assert_equal(result["text"], "Hello\nWorld\tTest", "json_decode correctly unescapes")
    
    unicode_json = "\{\"emoji\": \"\\u2764\"\}"
    result2 = codecs.json_decode(unicode_json)
    is_table2 = type(result2) == "table"
    assert_equal(is_table2, true, "json_decode handles unicode escapes")

// test: csv_decode handles quoted fields with embedded commas
function csv_quoted_fields()
    quoted_csv = "name,description\n\"Smith, John\",\"Engineer\""
    result = codecs.csv_decode(quoted_csv)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "csv_decode handles quoted fields")
    if is_table == true
        assert_equal(result[1]["name"], "Smith, John", "csv_decode quoted name with comma")
        assert_equal(result[1]["description"], "Engineer", "csv_decode quoted description")

// test: xml_decode parses deeply nested structures
function xml_nested_structure()
    nested_xml = "<root><items><item id=\"1\"><name>Test</name></item></items></root>"
    result = codecs.xml_decode(nested_xml)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "xml_decode nested structure")
    if is_table == true
        assert_equal(result["__tag"], "root", "xml_decode nested root tag")
        assert_equal(result[1]["__tag"], "items", "xml_decode nested items tag")

// test: json_encode escapes special characters properly
function json_encode_escaped_strings()
    special_str = "Hello\n\"World\"\tTab"
    json_str = codecs.json_encode(special_str)
    assert_equal(type(json_str), "string", "json_encode escapes special characters")
    
    parsed = codecs.json_decode(json_str)
    assert_equal(parsed, special_str, "json_encode roundtrip escaped string")

// test: csv_encode and csv_decode with custom delimiter
function csv_encode_custom_delimiter()
    data = [
        ["id" = 1, "name" = "Alice"],
        ["id" = 2, "name" = "Bob"]
    ]
    csv_string = codecs.csv_encode(data, true, ";")
    assert_equal(type(csv_string), "string", "csv_encode custom delimiter")
    read_data = codecs.csv_decode(csv_string, true, ";")
    assert_equal(read_data[1]["id"], 1, "csv_decode custom delimiter")

// execute all codecs library test cases
base64_roundtrip()
base64url_roundtrip()
hex_roundtrip()
hex_utf8_text()
hex_binary_data()
hex_case_insensitive()
hex_with_spaces()
hex_odd_characters()
hex_empty_input()
hex_invalid_characters()
csv_encode_read()
json_primitives()
json_array()
json_object()
json_decode()
xml_decode()
xml_encode()
json_invalid_input()
base64_invalid_input()
base64url_invalid_input()
csv_invalid_input()
xml_invalid_input()
json_escape_handling()
csv_quoted_fields()
xml_nested_structure()
json_encode_escaped_strings()
csv_encode_custom_delimiter()

// report completion of codecs library tests
os.output("[PASS] libraries/codecs.apex")