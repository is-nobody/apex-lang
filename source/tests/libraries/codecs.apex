import os
import codecs
import table

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function base64_roundtrip()
    text = "Hello, World! This is a test of Base64."
    b64_str = codecs.base_write(text)
    decoded_b64 = codecs.base_read(b64_str)
    assert_equal(decoded_b64, text, "base64 roundtrip")

function base64url_roundtrip()
    url_data = "data?param=value&other=123"
    b64url_str = codecs.baseurl_write(url_data)
    decoded_b64url = codecs.baseurl_read(b64url_str)
    assert_equal(decoded_b64url, url_data, "base64url roundtrip")

function csv_write_read()
    data = [
        ["id" = 1, "name" = "Alice", "age" = 30, "active" = true],
        ["id" = 2, "name" = "Bob, Jr.", "age" = 25, "active" = false],
        ["id" = 3, "name" = "Charlie", "age" = 40, "active" = true]
    ]
    
    csv_string = codecs.csv_write(data)
    read_data = codecs.csv_read(csv_string)
    
    is_table = type(read_data) == "table"
    assert_equal(is_table, true, "csv_read returns table")
    if is_table == true
        assert_equal(read_data[1]["name"], "Alice", "csv row 1 name")
        assert_equal(read_data[2]["name"], "Bob, Jr.", "csv row 2 name")
        assert_equal(read_data[3]["name"], "Charlie", "csv row 3 name")

function json_primitives()
    assert_equal(codecs.json_write(42), "42", "json_write number")
    assert_equal(codecs.json_write("hello"), "\"hello\"", "json_write string")
    assert_equal(codecs.json_write(true), "true", "json_write true")

function json_array()
    arr = ["apple", "banana", "cherry"]
    json_arr = codecs.json_write(arr)
    assert_equal(json_arr, "[\"apple\", \"banana\", \"cherry\"]", "json_write array")

function json_object()
    obj = ["name" = "Apex", "version" = 1.0, "is_cool" = true]
    json_obj = codecs.json_write(obj)
    readd = codecs.json_read(json_obj)
    assert_equal(readd["name"], "Apex", "json object roundtrip name")
    assert_equal(readd["is_cool"], true, "json object roundtrip is_cool")

function json_read()
    original = ["status" = "ok", "codes" = [200, 404]]
    raw_json = codecs.json_write(original)
    data = codecs.json_read(raw_json)
    is_table = type(data) == "table"
    assert_equal(is_table, true, "json_read returns table")
    if is_table == true
        assert_equal(data["status"], "ok", "json read status")
        assert_equal(data["codes"][1], 200, "json read first code")

function xml_read()
    simple_xml = "<root><item id=\"1\">Apple</item><item id=\"2\">Banana</item></root>"
    data_xml = codecs.xml_read(simple_xml)
    is_table = type(data_xml) == "table"
    assert_equal(is_table, true, "xml_read returns table")
    if is_table == true
        assert_equal(data_xml["__tag"], "root", "xml root tag")
        assert_equal(data_xml[1]["#text"], "Apple", "xml first item text")
        assert_equal(data_xml[1]["@id"], "1", "xml first item id")

function xml_write()
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
    writed_xml = codecs.xml_write(xml_obj)
    is_string = type(writed_xml) == "string"
    assert_equal(is_string, true, "xml_write returns string")

function json_invalid_input()
    invalid_json = "\{name: \"test\"\}"
    result = codecs.json_read(invalid_json)
    assert_equal(type(result), "none", "json_read invalid input returns none")
    
    invalid_json2 = "\{\"name\": \"test\",\}"
    result2 = codecs.json_read(invalid_json2)
    assert_equal(type(result2), "none", "json_read trailing comma returns none")

function base64_invalid_input()
    invalid_b64 = "ABC!DEF"
    result = codecs.base_read(invalid_b64)
    assert_equal(type(result), "none", "base_read invalid char returns none")
    
    invalid_b64_2 = "ABCD="
    result2 = codecs.base_read(invalid_b64_2)
    assert_equal(type(result2), "none", "base_read wrong padding returns none")

function base64url_invalid_input()
    invalid_b64url = "ABC!DEF"
    result = codecs.baseurl_read(invalid_b64url)
    assert_equal(type(result), "none", "baseurl_read invalid char returns none")

function csv_invalid_input()
    empty_csv = ""
    result = codecs.csv_read(empty_csv)
    assert_equal(type(result), "none", "csv_read empty input returns none")

function xml_invalid_input()
    invalid_xml = "<root><item>Test</root>"
    result = codecs.xml_read(invalid_xml)
    assert_equal(type(result), "none", "xml_read invalid XML returns none")

function json_escape_handling()
    escaped_json = "\{\"text\": \"Hello\\nWorld\\tTest\"\}"
    result = codecs.json_read(escaped_json)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "json_read handles escaped characters")
    if is_table == true
        assert_equal(result["text"], "Hello\nWorld\tTest", "json_read correctly unescapes")
    
    unicode_json = "\{\"emoji\": \"\\u2764\"\}"
    result2 = codecs.json_read(unicode_json)
    is_table2 = type(result2) == "table"
    assert_equal(is_table2, true, "json_read handles unicode escapes")

function csv_quoted_fields()
    quoted_csv = "name,description\n\"Smith, John\",\"Engineer\""
    result = codecs.csv_read(quoted_csv)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "csv_read handles quoted fields")
    if is_table == true
        assert_equal(result[1]["name"], "Smith, John", "csv_read quoted name with comma")
        assert_equal(result[1]["description"], "Engineer", "csv_read quoted description")

function xml_nested_structure()
    nested_xml = "<root><items><item id=\"1\"><name>Test</name></item></items></root>"
    result = codecs.xml_read(nested_xml)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "xml_read nested structure")
    if is_table == true
        assert_equal(result["__tag"], "root", "xml_read nested root tag")
        assert_equal(result[1]["__tag"], "items", "xml_read nested items tag")

function json_write_escaped_strings()
    special_str = "Hello\n\"World\"\tTab"
    json_str = codecs.json_write(special_str)
    assert_equal(type(json_str), "string", "json_write escapes special characters")
    
    parsed = codecs.json_read(json_str)
    assert_equal(parsed, special_str, "json_write roundtrip escaped string")

function csv_write_custom_delimiter()
    data = [
        ["id" = 1, "name" = "Alice"],
        ["id" = 2, "name" = "Bob"]
    ]
    csv_string = codecs.csv_write(data, true, ";")
    assert_equal(type(csv_string), "string", "csv_write custom delimiter")
    read_data = codecs.csv_read(csv_string, true, ";")
    assert_equal(read_data[1]["id"], 1, "csv_read custom delimiter")

base64_roundtrip()
base64url_roundtrip()
csv_write_read()
json_primitives()
json_array()
json_object()
json_read()
xml_read()
xml_write()
json_invalid_input()
base64_invalid_input()
base64url_invalid_input()
csv_invalid_input()
xml_invalid_input()
json_escape_handling()
csv_quoted_fields()
xml_nested_structure()
json_write_escaped_strings()
csv_write_custom_delimiter()

if tests_failed == 0
    os.output("[OK] libraries/codecs.apex")