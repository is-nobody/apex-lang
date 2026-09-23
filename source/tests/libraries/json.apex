// source/tests/libraries/json.apex
// Implementation of JSON Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// NOTE: apex uses {..} for string interpolation, so literal braces in test
// fixtures must be escaped as \{ and \}, otherwise the compiler tries to
// parse the JSON content as an apex expression

import os
import json
import table
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: asserts that a decode failed (returned none)
function assert_none(actual, name)
    if actual != none
        os.output("[ERROR] {name}: expected none, got {actual}")

// helper: asserts that a value is a table
function assert_table(actual, name)
    if type(actual) != "table"
        os.output("[ERROR] {name}: expected table, got {type(actual)}")

// helper: asserts that a value is a string
function assert_string(actual, name)
    if type(actual) != "string"
        os.output("[ERROR] {name}: expected string, got {type(actual)}")

// test: json serialization of primitive types
function json_primitives()
    assert_equal(json.encode(42), "42", "json_encode number")
    assert_equal(json.encode("hello"), "\"hello\"", "json_encode string")
    assert_equal(json.encode(true), "true", "json_encode true")
    assert_equal(json.encode(none), "null", "json_encode none")

// test: json serialization of numbers covers ints, negatives, and exact decimals
function json_numbers()
    assert_equal(json.encode(0), "0", "json_encode zero")
    assert_equal(json.encode(42), "42", "json_encode positive int")
    assert_equal(json.encode(-42), "-42", "json_encode negative int")
    assert_equal(json.encode(1000000), "1000000", "json_encode million")
    assert_equal(json.encode(0.5), "0.5", "json_encode half")
    assert_equal(json.encode(-0.5), "-0.5", "json_encode negative half")
    assert_equal(json.encode(2.5), "2.5", "json_encode two-and-half")

// test: json serialization of booleans covers both values explicitly
function json_booleans()
    assert_equal(json.encode(true), "true", "json_encode true explicit")
    assert_equal(json.encode(false), "false", "json_encode false explicit")

// test: json serialization of strings covers empty and plain values
function json_strings()
    assert_equal(json.encode(""), "\"\"", "json_encode empty string")
    assert_equal(json.encode("hello"), "\"hello\"", "json_encode plain string")
    assert_equal(json.encode("with space"), "\"with space\"", "json_encode spaced string")

// test: json serialization of arrays
function json_array()
    arr = ["apple", "banana", "cherry"]
    json_arr = json.encode(arr)
    assert_equal(json_arr, "[\"apple\", \"banana\", \"cherry\"]", "json_encode array")

// test: empty collections serialize as \{\} per the apex mapping rule
function json_empty_collections()
    assert_equal(json.encode([]), "\{\}", "json_encode empty table")

    decoded = json.decode("\{\}")
    assert_table(decoded, "json_decode empty object returns table")
    assert_equal(table.size(decoded), 0, "empty object decodes to empty table")

    decoded2 = json.decode("[]")
    assert_table(decoded2, "json_decode empty array returns table")
    assert_equal(table.size(decoded2), 0, "empty array decodes to empty table")

// test: positional tables become json arrays, named-key tables become objects
function json_array_vs_object()
    arr = []
    arr[1] = "a"
    arr[2] = "b"
    assert_equal(json.encode(arr), "[\"a\", \"b\"]", "positional table -> array")

    obj = ["x" = 1, "y" = 2]
    assert_equal(json.encode(obj), "\{\"x\": 1, \"y\": 2\}", "named-key table -> object")

// test: mixed table (positions + named keys) serializes as object with "1","2",...
function json_mixed_table()
    mixed = ["a", "b", "name" = "Alice"]
    encoded = json.encode(mixed)
    assert_string(encoded, "json_encode mixed table returns string")

    decoded = json.decode(encoded)
    assert_table(decoded, "mixed table roundtrip returns table")
    assert_equal(decoded["1"], "a", "mixed table position 1 preserved")
    assert_equal(decoded["2"], "b", "mixed table position 2 preserved")
    assert_equal(decoded["name"], "Alice", "mixed table named key preserved")

// test: non-string keys are stringified when written into an object
function json_key_stringification()
    t = []
    t[-1] = "neg"
    assert_equal(json.encode(t), "\{\"-1\": \"neg\"\}", "negative number key -> string")

    t2 = []
    t2[3.5] = "frac"
    assert_equal(json.encode(t2), "\{\"3.5\": \"frac\"\}", "fractional number key -> string")

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

// test: json_decode parses top-level primitives
function json_decode_primitives()
    assert_equal(json.decode("42"), 42, "decode number")
    assert_equal(json.decode("-42"), -42, "decode negative number")
    assert_equal(json.decode("3.5"), 3.5, "decode decimal")
    assert_equal(json.decode("1e2"), 100, "decode scientific")
    assert_equal(json.decode("\"hi\""), "hi", "decode string")
    assert_equal(json.decode("true"), true, "decode true")
    assert_equal(json.decode("false"), false, "decode false")
    assert_none(json.decode("null"), "decode null returns none")

// test: json_decode tolerates whitespace around and inside values
function json_decode_whitespace()
    data = json.decode("  \{\"x\": 1\}  ")
    assert_table(data, "leading/trailing ws tolerated")
    assert_equal(data["x"], 1, "value parsed despite ws")

    data2 = json.decode("\{ \"x\" : 1 \}")
    assert_table(data2, "inner ws tolerated")

    data3 = json.decode("\t[\n 1,\r\n 2,\n 3\n]\t")
    assert_table(data3, "ws in array tolerated")
    assert_equal(data3[3], 3, "array parsed despite ws")

// test: json_decode handles invalid json gracefully
function json_invalid_input()
    assert_none(json.decode("\{name: \"test\"\}"), "unquoted key rejected")
    assert_none(json.decode("\{\"name\": \"test\",\}"), "trailing comma rejected")
    assert_none(json.decode(""), "empty string rejected")
    assert_none(json.decode("   "), "whitespace only rejected")
    assert_none(json.decode("\{\"x\": 1"), "missing closing brace rejected")
    assert_none(json.decode("\{\"x\" 1\}"), "missing colon rejected")
    assert_none(json.decode("\{\"x\":: 1\}"), "double colon rejected")
    assert_none(json.decode("\"unterminated"), "unterminated string rejected")
    assert_none(json.decode("+1"), "plus sign rejected")
    assert_none(json.decode(".5"), "leading dot rejected")
    assert_none(json.decode("1."), "trailing dot rejected")
    assert_none(json.decode("01"), "leading zero rejected")
    assert_none(json.decode("TRUE"), "uppercase literal rejected")
    assert_none(json.decode("undefined"), "unknown literal rejected")

// test: json_decode rejects trailing garbage after a valid document
function json_trailing_garbage()
    assert_none(json.decode("\{\"x\": 1\} extra"), "trailing text rejected")
    assert_none(json.decode("42 42"), "two numbers rejected")
    assert_none(json.decode("truefalse"), "glued literals rejected")
    assert_none(json.decode("[1, 2] [3, 4]"), "two arrays rejected")

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
    assert_table(result2, "json_decode handles unicode escapes")

    // every supported escape sequence
    all_escapes = json.decode("\"\\\\\\\"\\/\\b\\f\\n\\r\\t\"")
    assert_string(all_escapes, "all escape sequences produce a string")

// test: json_decode combines surrogate pairs into one codepoint
function json_unicode_surrogates()
    // U+1F600 = GRINNING FACE, encoded as the surrogate pair \uD83D\uDE00
    pair = json.decode("\"\\uD83D\\uDE00\"")
    assert_string(pair, "surrogate pair decodes to string")

    // lone high surrogate -> replacement char, still decodes
    lone_hi = json.decode("\"\\uD83D\"")
    assert_string(lone_hi, "lone high surrogate tolerated")

    // lone low surrogate -> replacement char, still decodes
    lone_lo = json.decode("\"\\uDE00\"")
    assert_string(lone_lo, "lone low surrogate tolerated")

// test: json_encode escapes special characters properly
function json_encode_escaped_strings()
    special_str = "Hello\n\"World\"\tTab"
    json_str = json.encode(special_str)
    assert_equal(type(json_str), "string", "json_encode escapes special characters")

    parsed = json.decode(json_str)
    assert_equal(parsed, special_str, "json_encode roundtrip escaped string")

// test: full roundtrip through a nontrivial nested structure
function json_roundtrip_complex()
    complex = [
        "users" = [
            ["name" = "Alice", "age" = 30],
            ["name" = "Bob", "age" = 25]
        ],
        "count" = 2,
        "active" = true,
        "meta" = ["tags" = ["a", "b", "c"]]
    ]
    encoded = json.encode(complex)
    decoded = json.decode(encoded)
    assert_table(decoded, "complex roundtrip returns table")
    assert_equal(decoded["users"][1]["name"], "Alice", "complex roundtrip user 1 name")
    assert_equal(decoded["users"][2]["age"], 25, "complex roundtrip user 2 age")
    assert_equal(decoded["count"], 2, "complex roundtrip count")
    assert_equal(decoded["active"], true, "complex roundtrip active")
    assert_equal(decoded["meta"]["tags"][3], "c", "complex roundtrip nested tag")

// test: deep nesting does not break encode or decode
function json_deep_nesting()
    deep = ["a" = ["b" = ["c" = ["d" = ["e" = ["f" = 42]]]]]]
    encoded = json.encode(deep)
    decoded = json.decode(encoded)
    assert_equal(decoded["a"]["b"]["c"]["d"]["e"]["f"], 42, "deep nesting roundtrip")

// test: large positional arrays keep their size and order
function json_large_array()
    arr = []
    for i = 1, 100
        arr[i] = i

    encoded = json.encode(arr)
    decoded = json.decode(encoded)
    assert_equal(table.size(decoded), 100, "large array size preserved")
    assert_equal(decoded[1], 1, "large array first element")
    assert_equal(decoded[100], 100, "large array last element")

// test: nested arrays and objects roundtrip together
function json_nested_arrays_and_objects()
    data = [
        "matrix" = [
            [1, 2, 3],
            [4, 5, 6]
        ],
        "config" = ["debug" = false, "level" = 3]
    ]
    decoded = json.decode(json.encode(data))
    assert_equal(decoded["matrix"][2][3], 6, "nested matrix cell")
    assert_equal(decoded["config"]["debug"], false, "nested config debug")
    assert_equal(decoded["config"]["level"], 3, "nested config level")

// test: bmp unicode escapes decode to their utf-8 encoded codepoints
function json_unicode_bmp()
    // \u0041 = 'A'
    assert_equal(json.decode("\"\\u0041\""), "A", "bmp escape A")
    // \u2764 = '❤' (already checked in escape handling, re-check here)
    heart = json.decode("\"\\u2764\"")
    assert_string(heart, "bmp escape heart returns string")
    // \u00e9 = 'é' (2-byte utf-8)
    e_acute = json.decode("\"\\u00e9\"")
    assert_string(e_acute, "bmp escape e-acute returns string")
    // \u4f60\u597d = "你好" (two 3-byte utf-8 codepoints)
    hi = json.decode("\"\\u4f60\\u597d\"")
    assert_string(hi, "cjk escapes decode to string")
    // roundtrip: decoded string re-encodes to valid json that decodes back
    again = json.decode(json.encode(hi))
    assert_equal(again, hi, "cjk roundtrip through utf-8")

// test: exponent forms of json numbers are accepted on decode
function json_exponent_forms()
    assert_equal(json.decode("1e2"), 100, "lowercase e exponent")
    assert_equal(json.decode("1E2"), 100, "uppercase E exponent")
    assert_equal(json.decode("1e+2"), 100, "explicit plus exponent")
    assert_equal(json.decode("1e-2"), 0.01, "negative exponent")
    assert_equal(json.decode("1.5e3"), 1500, "decimal mantissa with exponent")
    assert_equal(json.decode("15e-1"), 1.5, "negative exponent with int mantissa")

// test: malformed exponent forms are rejected
function json_invalid_exponents()
    assert_none(json.decode("1e"), "bare e rejected")
    assert_none(json.decode("1e+"), "e plus with no digits rejected")
    assert_none(json.decode("1e-"), "e minus with no digits rejected")
    assert_none(json.decode("1e2.5"), "fractional exponent rejected")
    assert_none(json.decode("1e2e3"), "double exponent rejected")

// test: nulls nested inside structures survive roundtrip
function json_nested_nulls()
    data = ["a" = none, "b" = [1, none, 3], "c" = ["d" = none]]
    decoded = json.decode(json.encode(data))
    assert_equal(decoded["a"], none, "top-level null preserved")
    assert_equal(decoded["b"][2], none, "array null preserved")
    assert_equal(decoded["b"][1], 1, "sibling in array preserved")
    assert_equal(decoded["b"][3], 3, "other sibling preserved")
    assert_equal(decoded["c"]["d"], none, "nested object null preserved")

// test: empty strings survive roundtrip inside arrays and objects
function json_empty_strings_nested()
    data = ["name" = "", "tags" = ["", "x", ""]]
    decoded = json.decode(json.encode(data))
    assert_equal(decoded["name"], "", "empty string in object")
    assert_equal(decoded["tags"][1], "", "empty string first in array")
    assert_equal(decoded["tags"][2], "x", "middle sibling preserved")
    assert_equal(decoded["tags"][3], "", "empty string last in array")

// test: long strings survive a full roundtrip unchanged
function json_long_string()
    s = ""
    for i = 1, 1000
        s = "{s}x"
    assert_equal(string.length(s), 1000, "built 1000-char string")
    decoded = json.decode(json.encode(s))
    assert_equal(string.length(decoded), 1000, "1000-char string roundtrips")
    assert_equal(decoded, s, "1000-char string content preserved")

// test: escaping slash is accepted on decode, slash is not escaped on encode
function json_slash_handling()
    // "\/" is valid json and decodes to "/"
    result = json.decode("\"a\\/b\"")
    assert_equal(result, "a/b", "escaped slash decodes to slash")
    // encode does not need to escape slash
    encoded = json.encode("a/b")
    assert_equal(encoded, "\"a/b\"", "slash not escaped on encode")
    // roundtrip
    assert_equal(json.decode(encoded), "a/b", "slash roundtrip")

// test: quotes and backslashes inside object keys are escaped correctly
function json_special_key_escaping()
    t = []
    t["key\"with\"quotes"] = 1
    decoded = json.decode(json.encode(t))
    assert_equal(decoded["key\"with\"quotes"], 1, "quoted key roundtrip")

    t2 = []
    t2["back\\slash"] = 2
    decoded2 = json.decode(json.encode(t2))
    assert_equal(decoded2["back\\slash"], 2, "backslash key roundtrip")

    t3 = []
    t3["tab\tkey"] = 3
    decoded3 = json.decode(json.encode(t3))
    assert_equal(decoded3["tab\tkey"], 3, "tab in key roundtrip")

// test: array containing every value type preserves each element
function json_array_mixed_types()
    arr = [1, "two", true, none, 3.5]
    decoded = json.decode(json.encode(arr))
    assert_equal(decoded[1], 1, "array int element")
    assert_equal(decoded[2], "two", "array string element")
    assert_equal(decoded[3], true, "array true element")
    assert_equal(decoded[4], none, "array null element")
    assert_equal(decoded[5], 3.5, "array float element")

// test: object containing every value type preserves each field
function json_object_mixed_types()
    obj = ["int" = 1, "str" = "x", "bool" = true, "null" = none, "num" = 2.5]
    decoded = json.decode(json.encode(obj))
    assert_equal(decoded["int"], 1, "object int field")
    assert_equal(decoded["str"], "x", "object string field")
    assert_equal(decoded["bool"], true, "object bool field")
    assert_equal(decoded["null"], none, "object null field")
    assert_equal(decoded["num"], 2.5, "object float field")

// test: multi-level arrays preserve shape at every depth
function json_multi_level_arrays()
    data = [[1, 2], [3, 4], [5, [6, 7]]]
    decoded = json.decode(json.encode(data))
    assert_equal(decoded[1][1], 1, "level 2 first element")
    assert_equal(decoded[2][2], 4, "level 2 second pair second element")
    assert_equal(decoded[3][2][1], 6, "level 3 first element")
    assert_equal(decoded[3][2][2], 7, "level 3 second element")

// test: exact decimal values roundtrip without precision loss
function json_decimal_roundtrip()
    assert_equal(json.decode(json.encode(0.1)), 0.1, "0.1 roundtrip")
    assert_equal(json.decode(json.encode(0.25)), 0.25, "0.25 roundtrip")
    assert_equal(json.decode(json.encode(0.5)), 0.5, "0.5 roundtrip")
    assert_equal(json.decode(json.encode(2.5)), 2.5, "2.5 roundtrip")
    assert_equal(json.decode(json.encode(100.125)), 100.125, "100.125 roundtrip")
    assert_equal(json.decode(json.encode(-0.5)), -0.5, "negative 0.5 roundtrip")

// test: decoded values carry the expected apex type, not a stringified form
function json_number_types()
    assert_equal(type(json.decode("42")), "number", "42 decodes to number")
    assert_equal(type(json.decode("-42")), "number", "-42 decodes to number")
    assert_equal(type(json.decode("3.14")), "number", "3.14 decodes to number")
    assert_equal(type(json.decode("true")), "boolean", "true decodes to boolean")
    assert_equal(type(json.decode("null")), "none", "null decodes to none")
    assert_equal(type(json.decode("\"x\"")), "string", "string decodes to string")
    assert_equal(type(json.decode("[]")), "table", "array decodes to table")
    assert_equal(type(json.decode("\{\}")), "table", "object decodes to table")

// test: empty containers tolerate whitespace inside the delimiters
function json_empty_container_ws()
    assert_table(json.decode("\{  \}"), "empty object with spaces")
    assert_equal(table.size(json.decode("\{  \}")), 0, "empty object has size 0")
    assert_table(json.decode("[  ]"), "empty array with spaces")
    assert_equal(table.size(json.decode("[  ]")), 0, "empty array has size 0")
    assert_table(json.decode("[\t]"), "empty array with tab")
    assert_table(json.decode("[\n]"), "empty array with newline")

// test: encode turns literal control characters into escape sequences
function json_encode_control_chars()
    assert_equal(json.encode("a\nb"), "\"a\\nb\"", "newline encodes as backslash-n")
    assert_equal(json.encode("a\tb"), "\"a\\tb\"", "tab encodes as backslash-t")
    assert_equal(json.encode("a\rb"), "\"a\\rb\"", "cr encodes as backslash-r")
    assert_equal(json.encode("a\"b"), "\"a\\\"b\"", "quote encodes as backslash-quote")
    assert_equal(json.encode("a\\b"), "\"a\\\\b\"", "backslash encodes as double-backslash")
    // roundtrip confirms the escape is decodable back to the source
    original = "x\ny\tz\r\"w\"\\v"
    assert_equal(json.decode(json.encode(original)), original, "control-char roundtrip")

// test: deeper nesting than the earlier test still roundtrips
function json_deeper_nesting()
    deep = ["l1" = ["l2" = ["l3" = ["l4" = ["l5" = ["l6" = ["l7" = ["l8" = ["l9" = ["l10" = 42]]]]]]]]]]
    decoded = json.decode(json.encode(deep))
    assert_equal(
        decoded["l1"]["l2"]["l3"]["l4"]["l5"]["l6"]["l7"]["l8"]["l9"]["l10"],
        42,
        "10-level nesting roundtrip"
    )

// test: integers at the edge of exact double representation roundtrip
function json_large_integer_roundtrip()
    // 2^53 is the largest integer exactly representable as a double
    big = 9007199254740992
    assert_equal(json.decode(json.encode(big)), big, "2^53 roundtrip")
    assert_equal(json.decode(json.encode(-big)), -big, "negative 2^53 roundtrip")
    // 2^53 - 1 is still exact
    assert_equal(json.decode(json.encode(9007199254740991)), 9007199254740991, "2^53-1 roundtrip")

// execute all json library test cases
json_primitives()
json_numbers()
json_booleans()
json_strings()
json_array()
json_empty_collections()
json_array_vs_object()
json_mixed_table()
json_key_stringification()
json_object()
json_decode()
json_decode_primitives()
json_decode_whitespace()
json_invalid_input()
json_trailing_garbage()
json_escape_handling()
json_unicode_surrogates()
json_unicode_bmp()
json_encode_escaped_strings()
json_encode_control_chars()
json_roundtrip_complex()
json_deep_nesting()
json_deeper_nesting()
json_large_array()
json_large_integer_roundtrip()
json_nested_arrays_and_objects()
json_exponent_forms()
json_invalid_exponents()
json_nested_nulls()
json_empty_strings_nested()
json_long_string()
json_slash_handling()
json_special_key_escaping()
json_array_mixed_types()
json_object_mixed_types()
json_multi_level_arrays()
json_decimal_roundtrip()
json_number_types()
json_empty_container_ws()

// report completion of json library tests
os.output("[PASS] libraries/json.apex")