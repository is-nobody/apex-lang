// source/tests/libraries/xml.apex
// Implementation of XML Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import xml
import table
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: asserts that a value is a table
function assert_table(actual, name)
    if type(actual) != "table"
        os.output("[ERROR] {name}: expected table, got {type(actual)}")

// helper: asserts that a value is a string
function assert_string(actual, name)
    if type(actual) != "string"
        os.output("[ERROR] {name}: expected string, got {type(actual)}")

// helper: asserts that a value is none
function assert_none(actual, name)
    if actual != none
        os.output("[ERROR] {name}: expected none, got {actual}")

// test: xml read parses basic xml structure (original)
function xml_decode()
    simple_xml = "<root><item id=\"1\">Apple</item><item id=\"2\">Banana</item></root>"
    data_xml = xml.decode(simple_xml)
    is_table = type(data_xml) == "table"
    assert_equal(is_table, true, "xml_decode returns table")
    if is_table == true
        assert_equal(data_xml["__tag"], "root", "xml root tag")
        assert_equal(data_xml[1]["#text"], "Apple", "xml first item text")
        assert_equal(data_xml[1]["@id"], "1", "xml first item id")

// test: decode a self-closing element yields only __tag
function xml_decode_self_closing()
    r = xml.decode("<a/>")
    assert_table(r, "self-closing decodes to table")
    assert_equal(r["__tag"], "a", "self-closing tag name")
    assert_equal(r["#text"], none, "self-closing has no text")
    assert_equal(r[1], none, "self-closing has no children")

// test: decode self-closing with spaces before '/'
function xml_decode_self_closing_spaced()
    r = xml.decode("<a   />")
    assert_table(r, "spaced self-close decodes")
    assert_equal(r["__tag"], "a", "spaced self-close tag name")

// test: decode self-closing with spaces around '/'
function xml_decode_self_closing_extra_ws()
    r = xml.decode("<a / >")
    assert_table(r, "self-close extra ws decodes")
    assert_equal(r["__tag"], "a", "self-close extra ws tag")

// test: decode self-closing with space after attributes
function xml_decode_self_closing_spaced_attrs()
    r = xml.decode("<a x=\"1\" />")
    assert_equal(r["@x"], "1", "self-close spaced attr value")

// test: decode an empty paired element
function xml_decode_empty_paired()
    r = xml.decode("<a></a>")
    assert_table(r, "empty paired decodes")
    assert_equal(r["__tag"], "a", "empty paired tag name")
    assert_equal(r["#text"], none, "empty paired has no text")

// test: decode an element with plain text
function xml_decode_text()
    r = xml.decode("<a>hello</a>")
    assert_table(r, "text decodes to table")
    assert_equal(r["__tag"], "a", "text tag name")
    assert_equal(r["#text"], "hello", "text content")

// test: decode preserves whitespace inside text content
function xml_decode_text_whitespace()
    r = xml.decode("<a>  hello  </a>")
    assert_equal(r["#text"], "  hello  ", "text whitespace preserved")

// test: decode drops whitespace-only text
function xml_decode_ws_only_text()
    r = xml.decode("<a>   </a>")
    assert_equal(r["#text"], none, "ws-only text dropped")
    r2 = xml.decode("<a>\n\t  </a>")
    assert_equal(r2["#text"], none, "newline-ws-only text dropped")

// test: decode drops whitespace between child elements
function xml_decode_inter_element_ws()
    r = xml.decode("<a>\n  <b/>\n  <c/>\n</a>")
    assert_equal(r["#text"], none, "inter-element ws not stored as text")
    assert_equal(r[1]["__tag"], "b", "inter-element child b")
    assert_equal(r[2]["__tag"], "c", "inter-element child c")

// test: decode preserves non-whitespace text alongside children
function xml_decode_text_with_children()
    r = xml.decode("<a>hi<b/></a>")
    assert_equal(r["#text"], "hi", "text with children preserved")
    assert_equal(r[1]["__tag"], "b", "child with text preserved")

// test: decode a single attribute
function xml_decode_single_attribute()
    r = xml.decode("<a x=\"1\"/>")
    assert_equal(r["@x"], "1", "single attribute value")

// test: decode multiple attributes
function xml_decode_multiple_attributes()
    r = xml.decode("<a x=\"1\" y=\"2\" z=\"3\"/>")
    assert_equal(r["@x"], "1", "attribute x")
    assert_equal(r["@y"], "2", "attribute y")
    assert_equal(r["@z"], "3", "attribute z")

// test: decode attributes with spaces around the equals sign
function xml_decode_attr_spaces_around_equals()
    r = xml.decode("<a x = \"1\" y = \"2\"/>")
    assert_equal(r["@x"], "1", "spaces around equals attr x")
    assert_equal(r["@y"], "2", "spaces around equals attr y")

// test: decode attribute with empty value
function xml_decode_attr_empty()
    r = xml.decode("<a x=\"\"/>")
    assert_equal(r["@x"], "", "empty attribute value")

// test: decode attribute value entities are kept raw
function xml_decode_attr_entities_raw()
    r = xml.decode("<a x=\"a &amp; b\"/>")
    assert_equal(r["@x"], "a &amp; b", "attribute entity not decoded")

// test: decode attribute names with every allowed separator
function xml_decode_attr_chars()
    r = xml.decode("<a my-attr=\"1\" my_attr=\"2\" my.attr=\"3\" my:attr=\"4\"/>")
    assert_equal(r["@my-attr"], "1", "attr with hyphen")
    assert_equal(r["@my_attr"], "2", "attr with underscore")
    assert_equal(r["@my.attr"], "3", "attr with dot")
    assert_equal(r["@my:attr"], "4", "attr with colon")

// test: decode rejects invalid attribute names
function xml_decode_invalid_attr_name()
    assert_none(xml.decode("<a 1x=\"1\"/>"), "attr starting with digit")
    assert_none(xml.decode("<a -x=\"1\"/>"), "attr starting with hyphen")

// test: decode single child
function xml_decode_single_child()
    r = xml.decode("<a><b/></a>")
    assert_table(r[1], "single child is table")
    assert_equal(r[1]["__tag"], "b", "single child tag")

// test: decode multiple children preserves order
function xml_decode_multiple_children()
    r = xml.decode("<a><b/><c/><d/></a>")
    assert_equal(r[1]["__tag"], "b", "child 1 tag")
    assert_equal(r[2]["__tag"], "c", "child 2 tag")
    assert_equal(r[3]["__tag"], "d", "child 3 tag")
    assert_equal(r[4], none, "no extra child")

// test: xml_decode parses deeply nested structures (original)
function xml_nested_structure()
    nested_xml = "<root><items><item id=\"1\"><name>Test</name></item></items></root>"
    result = xml.decode(nested_xml)
    is_table = type(result) == "table"
    assert_equal(is_table, true, "xml_decode nested structure")
    if is_table == true
        assert_equal(result["__tag"], "root", "xml_decode nested root tag")
        assert_equal(result[1]["__tag"], "items", "xml_decode nested items tag")
        assert_equal(result[1][1]["__tag"], "item", "nested item tag")
        assert_equal(result[1][1]["@id"], "1", "nested item id")
        assert_equal(result[1][1][1]["#text"], "Test", "nested name text")

// test: decode tag names with every allowed separator
function xml_decode_tag_chars()
    assert_equal(xml.decode("<my-tag/>")["__tag"], "my-tag", "tag with hyphen")
    assert_equal(xml.decode("<my_tag/>")["__tag"], "my_tag", "tag with underscore")
    assert_equal(xml.decode("<my.tag/>")["__tag"], "my.tag", "tag with dot")
    assert_equal(xml.decode("<my:tag/>")["__tag"], "my:tag", "tag with colon")
    assert_equal(xml.decode("<tag123/>")["__tag"], "tag123", "tag with digits")

// test: decode is case sensitive
function xml_decode_case_sensitive()
    r = xml.decode("<Root><Child/></Root>")
    assert_equal(r["__tag"], "Root", "uppercase tag preserved")
    assert_equal(r[1]["__tag"], "Child", "uppercase child tag preserved")

// test: decode preserves mixed text and attributes
function xml_decode_text_and_attr()
    r = xml.decode("<a x=\"1\">hello</a>")
    assert_equal(r["@x"], "1", "text+attr attribute")
    assert_equal(r["#text"], "hello", "text+attr text")

// test: decode skips leading whitespace before root
function xml_decode_leading_ws()
    r = xml.decode("  \n  <a/>")
    assert_table(r, "leading ws tolerated")
    assert_equal(r["__tag"], "a", "leading ws tag")

// test: decode closing tag tolerates trailing whitespace
function xml_decode_close_tag_trailing_ws()
    r = xml.decode("<a></a >")
    assert_equal(r["__tag"], "a", "close tag trailing ws")

// test: decode raw '&' in text is preserved
function xml_decode_raw_ampersand()
    r = xml.decode("<a>a & b</a>")
    assert_equal(r["#text"], "a & b", "raw & preserved")

// test: decode unescaped '<' inside text is rejected
function xml_decode_unescaped_lt()
    assert_none(xml.decode("<a>x < y</a>"), "unescaped < in text")

// test: xml_decode handles malformed xml gracefully (original)
function xml_invalid_input()
    invalid_xml = "<root><item>Test</root>"
    result = xml.decode(invalid_xml)
    assert_equal(type(result), "none", "xml_decode invalid XML returns none")

// test: decode empty input returns none
function xml_decode_empty()
    assert_none(xml.decode(""), "empty input")
    assert_none(xml.decode("   "), "whitespace-only input")
    assert_none(xml.decode("\n\t "), "newline-tab input")

// test: decode input without angle brackets returns none
function xml_decode_no_angles()
    assert_none(xml.decode("hello"), "plain text input")

// test: decode mismatched closing tag returns none
function xml_decode_mismatched()
    assert_none(xml.decode("<a></b>"), "mismatched tags")

// test: decode unclosed tag returns none
function xml_decode_unclosed()
    assert_none(xml.decode("<a>"), "unclosed tag")
    assert_none(xml.decode("<a"), "no closing angle")
    assert_none(xml.decode("<a><b></a>"), "unclosed child")

// test: decode invalid tag start returns none
function xml_decode_invalid_tag_start()
    assert_none(xml.decode("<1a/>"), "tag starting with digit")
    assert_none(xml.decode("<-a/>"), "tag starting with hyphen")
    assert_none(xml.decode("<.a/>"), "tag starting with dot")

// test: decode attribute without quotes returns none
function xml_decode_attr_no_quotes()
    assert_none(xml.decode("<a x=1/>"), "attribute without quotes")

// test: xml write serializes table to xml string (original)
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

// test: encode a self-closing element
function xml_encode_self_closing()
    r = xml.encode(["__tag" = "a"])
    assert_string(r, "self-closing encodes to string")
    assert_equal(r, "<a/>\n", "self-closing output")

// test: encode element with text content
function xml_encode_text()
    r = xml.encode(["__tag" = "a", "#text" = "hello"])
    assert_equal(r, "<a>hello</a>\n", "text output")

// test: encode element with a single attribute
function xml_encode_attributes()
    r = xml.encode(["__tag" = "a", "@x" = "1"])
    assert_equal(r, "<a x=\"1\"/>\n", "single attribute output")

// test: encode element with a single child
function xml_encode_single_child()
    r = xml.encode(["__tag" = "a", 1 = ["__tag" = "b"]])
    assert_equal(r, "<a>\n  <b/>\n</a>\n", "single child output")

// test: encode element with multiple children
function xml_encode_multiple_children()
    r = xml.encode([
        "__tag" = "a",
        1 = ["__tag" = "b"],
        2 = ["__tag" = "c"]
    ])
    assert_equal(r, "<a>\n  <b/>\n  <c/>\n</a>\n", "multiple children output")

// test: encode nested children
function xml_encode_nested()
    r = xml.encode([
        "__tag" = "a",
        1 = [
            "__tag" = "b",
            1 = ["__tag" = "c"]
        ]
    ])
    assert_equal(r, "<a>\n  <b>\n    <c/>\n  </b>\n</a>\n", "nested output")

// test: encode text plus child on separate lines
function xml_encode_text_and_child()
    r = xml.encode([
        "__tag" = "a",
        "#text" = "hi",
        1 = ["__tag" = "b"]
    ])
    assert_equal(r, "<a>hi\n  <b/>\n</a>\n", "text+child output")

// test: encode multiple attributes (order-independent via roundtrip)
function xml_encode_multiple_attributes()
    r = xml.encode(["__tag" = "a", "@x" = "1", "@y" = "2"])
    assert_string(r, "multiple attrs encodes to string")
    decoded = xml.decode(r)
    assert_equal(decoded["@x"], "1", "multiple attrs x roundtrip")
    assert_equal(decoded["@y"], "2", "multiple attrs y roundtrip")

// test: encode escapes text content
function xml_encode_text_escaping()
    r = xml.encode(["__tag" = "a", "#text" = "a & b < c > d"])
    assert_equal(r, "<a>a &amp; b &lt; c &gt; d</a>\n", "text escaping")

// test: encode escapes carriage return in text
function xml_encode_text_cr()
    r = xml.encode(["__tag" = "a", "#text" = "x\ry"])
    assert_equal(r, "<a>x&#13;y</a>\n", "carriage return in text")

// test: encode escapes attribute content
function xml_encode_attr_escaping()
    r = xml.encode(["__tag" = "a", "@x" = "a & b < c > d \"q\""])
    assert_equal(r, "<a x=\"a &amp; b &lt; c &gt; d &quot;q&quot;\"/>\n", "attr escaping")

// test: encode escapes whitespace in attributes
function xml_encode_attr_whitespace()
    r = xml.encode(["__tag" = "a", "@x" = "a\tb\nc\rd"])
    assert_equal(r, "<a x=\"a&#9;b&#10;c&#13;d\"/>\n", "attr whitespace escaping")

// test: encode drops non-string @ values silently
function xml_encode_attr_non_string()
    r = xml.encode(["__tag" = "a", "@x" = 42])
    assert_equal(r, "<a/>\n", "non-string attribute dropped")

// test: encode rejects an invalid tag name
function xml_encode_invalid_tag()
    assert_none(xml.encode(["__tag" = "1a"]), "tag starting with digit")
    assert_none(xml.encode(["__tag" = ""]), "empty tag")
    assert_none(xml.encode(["__tag" = "-a"]), "tag starting with hyphen")
    assert_none(xml.encode(["__tag" = ".a"]), "tag starting with dot")

// test: encode rejects tag names containing bad characters
function xml_encode_invalid_tag_chars()
    assert_none(xml.encode(["__tag" = "a b"]), "space in tag")
    assert_none(xml.encode(["__tag" = "a<b"]), "angle in tag")
    assert_none(xml.encode(["__tag" = "a/b"]), "slash in tag")

// test: encode rejects an invalid attribute name
function xml_encode_invalid_attr()
    assert_none(xml.encode(["__tag" = "a", "@1x" = "v"]), "attr starting with digit")
    assert_none(xml.encode(["__tag" = "a", "@" = "v"]), "empty attr name")
    assert_none(xml.encode(["__tag" = "a", "@-x" = "v"]), "attr starting with hyphen")

// test: encode returns none for a missing __tag
function xml_encode_missing_tag()
    assert_none(xml.encode(["x" = 1]), "missing __tag")
    assert_none(xml.encode([]), "empty table")

// test: encode accepts numeric string keys as children
function xml_encode_string_numeric_keys()
    r = xml.encode([
        "__tag" = "a",
        "1" = ["__tag" = "b"]
    ])
    assert_string(r, "numeric string key encodes")
    assert_equal(r, "<a>\n  <b/>\n</a>\n", "numeric string key child")

// test: encode emits children after gaps in numeric keys
function xml_encode_gap_in_numeric_keys()
    t = ["__tag" = "a"]
    t[1] = ["__tag" = "b"]
    t[3] = ["__tag" = "c"]
    r = xml.encode(t)
    assert_string(r, "gap in numeric keys encodes")
    decoded = xml.decode(r)
    assert_equal(decoded[1]["__tag"], "b", "gap child b present")
    assert_equal(decoded[2]["__tag"], "c", "gap child c present")

// test: roundtrip a simple element with text
function xml_roundtrip_text()
    original = ["__tag" = "a", "#text" = "hello"]
    decoded = xml.decode(xml.encode(original))
    assert_table(decoded, "roundtrip text table")
    assert_equal(decoded["__tag"], "a", "roundtrip text tag")
    assert_equal(decoded["#text"], "hello", "roundtrip text content")

// test: roundtrip an element with attributes
function xml_roundtrip_attributes()
    original = ["__tag" = "a", "@x" = "1", "@y" = "2"]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["@x"], "1", "roundtrip attr x")
    assert_equal(decoded["@y"], "2", "roundtrip attr y")

// test: roundtrip children
function xml_roundtrip_children()
    original = [
        "__tag" = "root",
        1 = ["__tag" = "a", "#text" = "first"],
        2 = ["__tag" = "b", "#text" = "second"]
    ]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["__tag"], "root", "roundtrip root tag")
    assert_equal(decoded[1]["__tag"], "a", "roundtrip child 1 tag")
    assert_equal(decoded[1]["#text"], "first", "roundtrip child 1 text")
    assert_equal(decoded[2]["__tag"], "b", "roundtrip child 2 tag")
    assert_equal(decoded[2]["#text"], "second", "roundtrip child 2 text")

// test: roundtrip a deeper nested structure
function xml_roundtrip_nested()
    original = [
        "__tag" = "root",
        1 = [
            "__tag" = "items",
            1 = [
                "__tag" = "item",
                "@id" = "1",
                "#text" = "value"
            ]
        ]
    ]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["__tag"], "root", "roundtrip nested root")
    assert_equal(decoded[1]["__tag"], "items", "roundtrip nested items")
    assert_equal(decoded[1][1]["__tag"], "item", "roundtrip nested item")
    assert_equal(decoded[1][1]["@id"], "1", "roundtrip nested item id")
    assert_equal(decoded[1][1]["#text"], "value", "roundtrip nested item text")

// test: encode(decode(encode(x))) == encode(x) for clean input
function xml_roundtrip_idempotent()
    original = [
        "__tag" = "config",
        "@version" = "1.0",
        1 = ["__tag" = "setting", "@name" = "theme", "#text" = "dark"],
        2 = ["__tag" = "setting", "@name" = "lang", "#text" = "en"]
    ]
    once = xml.encode(original)
    twice = xml.encode(xml.decode(once))
    assert_equal(once, twice, "encode(decode(encode(x))) == encode(x)")

// test: roundtrip preserves unicode text
function xml_roundtrip_unicode_text()
    original = ["__tag" = "a", "#text" = "你好 мир café 🎉"]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["#text"], "你好 мир café 🎉", "unicode text roundtrip")

// test: roundtrip preserves unicode attributes
function xml_roundtrip_unicode_attr()
    original = ["__tag" = "a", "@x" = "café"]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["@x"], "café", "unicode attribute roundtrip")

// test: roundtrip empty attribute value
function xml_roundtrip_empty_attr()
    original = ["__tag" = "a", "@x" = ""]
    decoded = xml.decode(xml.encode(original))
    assert_equal(decoded["@x"], "", "empty attr roundtrip")

// test: empty text encodes as paired tag but decodes back to no #text
function xml_roundtrip_empty_text()
    original = ["__tag" = "a", "#text" = ""]
    encoded = xml.encode(original)
    assert_equal(encoded, "<a></a>\n", "empty text encoded as paired tag")
    decoded = xml.decode(encoded)
    assert_equal(decoded["__tag"], "a", "empty text roundtrip tag")
    assert_equal(decoded["#text"], none, "empty text decodes to no text")

// test: long text survives a full roundtrip
function xml_roundtrip_long_text()
    s = ""
    for i = 1, 500
        s = "{s}x"
    assert_equal(string.length(s), 500, "built 500-char string")
    decoded = xml.decode(xml.encode(["__tag" = "a", "#text" = s]))
    assert_equal(string.length(decoded["#text"]), 500, "long text length preserved")
    assert_equal(decoded["#text"], s, "long text content preserved")

// test: many children roundtrip with order preserved
function xml_roundtrip_many_children()
    t = ["__tag" = "root"]
    for i = 1, 50
        t[i] = ["__tag" = "item", "@n" = string(i)]
    decoded = xml.decode(xml.encode(t))
    assert_equal(decoded[1]["@n"], "1", "many children first @n")
    assert_equal(decoded[50]["@n"], "50", "many children last @n")
    assert_equal(decoded[25]["__tag"], "item", "many children middle tag")

// test: deeply nested input is rejected instead of crashing (original)
function xml_depth_limit()
    n = 500                                        // well above XML_MAX_DEPTH (256)
    s = ""
    for i = 1, n
        s = "{s}<a>"
    for i = 1, n
        s = "{s}</a>"
    result = xml.decode(s)
    assert_equal(result, none, "deep nested xml rejected")

// test: encode rejects a pathologically nested table
function xml_encode_depth_limit()
    t = ["__tag" = "a"]
    for i = 1, 300
        t = ["__tag" = "a", 1 = t]
    result = xml.encode(t)
    assert_equal(result, none, "deep nested encode rejected")

// execute all xml library test cases
xml_decode()
xml_decode_self_closing()
xml_decode_self_closing_spaced()
xml_decode_self_closing_extra_ws()
xml_decode_self_closing_spaced_attrs()
xml_decode_empty_paired()
xml_decode_text()
xml_decode_text_whitespace()
xml_decode_ws_only_text()
xml_decode_inter_element_ws()
xml_decode_text_with_children()
xml_decode_single_attribute()
xml_decode_multiple_attributes()
xml_decode_attr_spaces_around_equals()
xml_decode_attr_empty()
xml_decode_attr_entities_raw()
xml_decode_attr_chars()
xml_decode_invalid_attr_name()
xml_decode_single_child()
xml_decode_multiple_children()
xml_nested_structure()
xml_decode_tag_chars()
xml_decode_case_sensitive()
xml_decode_text_and_attr()
xml_decode_leading_ws()
xml_decode_close_tag_trailing_ws()
xml_decode_raw_ampersand()
xml_decode_unescaped_lt()
xml_invalid_input()
xml_decode_empty()
xml_decode_no_angles()
xml_decode_mismatched()
xml_decode_unclosed()
xml_decode_invalid_tag_start()
xml_decode_attr_no_quotes()
xml_encode()
xml_encode_self_closing()
xml_encode_text()
xml_encode_attributes()
xml_encode_single_child()
xml_encode_multiple_children()
xml_encode_nested()
xml_encode_text_and_child()
xml_encode_multiple_attributes()
xml_encode_text_escaping()
xml_encode_text_cr()
xml_encode_attr_escaping()
xml_encode_attr_whitespace()
xml_encode_attr_non_string()
xml_encode_invalid_tag()
xml_encode_invalid_tag_chars()
xml_encode_invalid_attr()
xml_encode_missing_tag()
xml_encode_string_numeric_keys()
xml_encode_gap_in_numeric_keys()
xml_roundtrip_text()
xml_roundtrip_attributes()
xml_roundtrip_children()
xml_roundtrip_nested()
xml_roundtrip_idempotent()
xml_roundtrip_unicode_text()
xml_roundtrip_unicode_attr()
xml_roundtrip_empty_attr()
xml_roundtrip_empty_text()
xml_roundtrip_long_text()
xml_roundtrip_many_children()
xml_depth_limit()
xml_encode_depth_limit()

// report completion of xml library tests
os.output("[PASS] libraries/xml.apex")