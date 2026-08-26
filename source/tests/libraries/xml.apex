// source/tests/libraries/xml.apex
// Implementation of XML Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import xml
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: xml read parses basic xml structure
function xml_decode()
    simple_xml = "<root><item id=\"1\">Apple</item><item id=\"2\">Banana</item></root>"
    data_xml = xml.decode(simple_xml)
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
    writed_xml = xml.encode(xml_obj)
    is_string = type(writed_xml) == "string"
    assert_equal(is_string, true, "xml_encode returns string")

// test: xml_decode handles malformed xml gracefully
function xml_invalid_input()
    invalid_xml = "<root><item>Test</root>"
    result = xml.decode(invalid_xml)
    assert_equal(type(result), "none", "xml_decode invalid XML returns none")

// test: xml_decode parses deeply nested structures
function xml_nested_structure()
    nested_xml = "<root><items><item id=\"1\"><name>Test</name></item></items></root>"
    result = xml.decode(nested_xml)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "xml_decode nested structure")
    if is_table == true
        assert_equal(result["__tag"], "root", "xml_decode nested root tag")
        assert_equal(result[1]["__tag"], "items", "xml_decode nested items tag")

// execute all xml library test cases
xml_decode()
xml_encode()
xml_invalid_input()
xml_nested_structure()

// report completion of xml library tests
os.output("[PASS] libraries/xml.apex")