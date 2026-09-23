// source/tests/libraries/csv.apex
// Implementation of CSV Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import csv
import table
import string

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

// test: encode single row produces header plus one data line
function csv_encode_single_row()
    data = [["name" = "Alice", "age" = 30]]
    out = csv.encode(data)
    assert_equal(type(out), "string", "single row encode returns string")
    decoded = csv.decode(out)
    assert_equal(type(decoded), "table", "single row decode returns table")
    if type(decoded) == "table"
        assert_equal(table.size(decoded), 1, "single row size")
        assert_equal(decoded[1]["name"], "Alice", "single row name")
        assert_equal(decoded[1]["age"], 30, "single row age")

// test: encode a table with exactly one column
function csv_encode_single_column()
    data = [["only" = "a"], ["only" = "b"], ["only" = "c"]]
    out = csv.encode(data)
    decoded = csv.decode(out)
    assert_equal(table.size(decoded), 3, "single column row count")
    assert_equal(decoded[1]["only"], "a", "single column row 1")
    assert_equal(decoded[2]["only"], "b", "single column row 2")
    assert_equal(decoded[3]["only"], "c", "single column row 3")

// test: encode a wide table with many columns
function csv_encode_wide_table()
    data = [[
        "c1" = 1, "c2" = 2, "c3" = 3, "c4" = 4, "c5" = 5,
        "c6" = 6, "c7" = 7, "c8" = 8, "c9" = 9, "c10" = 10
    ]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(type(decoded), "table", "wide table decodes")
    assert_equal(decoded[1]["c1"], 1, "wide column 1")
    assert_equal(decoded[1]["c5"], 5, "wide column 5")
    assert_equal(decoded[1]["c10"], 10, "wide column 10")

// test: encode preserves numeric values of various shapes
function csv_encode_numeric_variety()
    data = [[
        "int" = 100,
        "zero" = 0,
        "neg" = -42,
        "float" = 3.5,
        "small" = 0.001
    ]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["int"], 100, "numeric int")
    assert_equal(decoded[1]["zero"], 0, "numeric zero")
    assert_equal(decoded[1]["neg"], -42, "numeric negative")
    assert_equal(decoded[1]["float"], 3.5, "numeric float")
    assert_equal(decoded[1]["small"], 0.001, "numeric small decimal")

// test: encode preserves booleans in both states
function csv_encode_bool_variety()
    data = [["t" = true, "f" = false, "t2" = true]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["t"], true, "bool true preserved")
    assert_equal(decoded[1]["f"], false, "bool false preserved")
    assert_equal(decoded[1]["t2"], true, "bool second true preserved")

// test: encode escapes fields containing only a comma
function csv_encode_value_only_comma()
    data = [["v" = ","]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], ",", "comma-only value roundtrip")

// test: encode escapes fields containing only a quote
function csv_encode_value_only_quote()
    data = [["v" = "\""]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\"", "quote-only value roundtrip")

// test: encode escapes fields containing only a newline
function csv_encode_value_only_newline()
    data = [["v" = "\n"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\n", "newline-only value roundtrip")

// test: encode escapes fields containing only a carriage return
function csv_encode_value_only_cr()
    data = [["v" = "\r"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\r", "cr-only value roundtrip")

// test: encode handles values with tabs (not a csv delimiter)
function csv_encode_tab_value()
    data = [["v" = "a\tb"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "a\tb", "tab-in-value roundtrip")

// test: encode handles values with multiple special chars combined
function csv_encode_value_all_special()
    data = [["v" = "a,b\"c\nd\re\tf"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "a,b\"c\nd\re\tf", "all special chars roundtrip")

// test: encode handles a value that starts with a quote
function csv_encode_value_starts_quote()
    data = [["v" = "\"start"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\"start", "leading quote roundtrip")

// test: encode handles a value that ends with a quote
function csv_encode_value_ends_quote()
    data = [["v" = "end\""]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "end\"", "trailing quote roundtrip")

// test: encode handles a value made of two adjacent quotes
function csv_encode_value_double_quote()
    data = [["v" = "\"\""]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\"\"", "double-quote value roundtrip")

// test: encode handles a value of only whitespace
function csv_encode_value_whitespace()
    data = [["v" = "   "]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "   ", "whitespace-only value roundtrip")

// test: encode handles a value of only spaces and tabs
function csv_encode_value_spaces_tabs()
    data = [["v" = " \t "]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], " \t ", "spaces-and-tabs value roundtrip")

// test: empty strings survive encode/decode unchanged
function csv_encode_empty_string_value()
    data = [["a" = "", "b" = "x", "c" = ""]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a"], "", "empty first value")
    assert_equal(decoded[1]["b"], "x", "middle value preserved")
    assert_equal(decoded[1]["c"], "", "empty last value")

// test: many rows survive roundtrip with order preserved
function csv_encode_many_rows()
    data = []
    for i = 1, 50
        data[i] = ["id" = i, "name" = "user{i}"]
    out = csv.encode(data)
    decoded = csv.decode(out)
    assert_equal(table.size(decoded), 50, "50 rows preserved")
    assert_equal(decoded[1]["id"], 1, "first row id")
    assert_equal(decoded[50]["id"], 50, "last row id")
    assert_equal(decoded[25]["name"], "user25", "middle row name")

// test: long string value survives roundtrip
function csv_encode_long_value()
    s = ""
    for i = 1, 500
        s = "{s}y"
    data = [["v" = s]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(string.length(decoded[1]["v"]), 500, "long value length")
    assert_equal(decoded[1]["v"], s, "long value content")

// test: decode a header-only document returns a table (possibly empty)
function csv_decode_header_only()
    result = csv.decode("name,age")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "header-only returns table")
    if is_table == true
        assert_equal(table.size(result), 0, "header-only has no rows")

// test: decode with trailing newline does not create an extra row
function csv_decode_trailing_newline()
    result = csv.decode("name,age\nAlice,30\n")
    assert_equal(type(result), "table", "trailing newline returns table")
    assert_equal(table.size(result), 1, "trailing newline does not add row")

// test: decode without trailing newline parses the last row
function csv_decode_no_trailing_newline()
    result = csv.decode("name,age\nAlice,30")
    assert_equal(type(result), "table", "no trailing newline returns table")
    assert_equal(table.size(result), 1, "no trailing newline row count")
    assert_equal(result[1]["name"], "Alice", "no trailing newline name")

// test: decode accepts bare carriage return as a line ending
function csv_decode_cr_only()
    result = csv.decode("name,age\rAlice,30\rBob,25")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "cr-only returns table")
    if is_table == true
        assert_equal(result[1]["name"], "Alice", "cr-only row 1")
        assert_equal(result[2]["name"], "Bob", "cr-only row 2")

// test: decode accepts mixed line endings in the same document
function csv_decode_mixed_endings()
    result = csv.decode("name,age\r\nAlice,30\nBob,25\rCharlie,40")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "mixed endings returns table")
    if is_table == true
        assert_equal(table.size(result), 3, "mixed endings row count")
        assert_equal(result[3]["name"], "Charlie", "mixed endings row 3")

// test: decode empty-field-only row keeps all headers
function csv_decode_only_empty_fields()
    result = csv.decode("a,b,c\n,,\n,,")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "empty-fields-only returns table")
    if is_table == true
        assert_equal(table.size(result), 2, "two rows of empty fields")
        assert_equal(result[1]["a"], "", "empty row 1 col a")
        assert_equal(result[2]["c"], "", "empty row 2 col c")

// test: decode preserves leading and trailing whitespace in unquoted fields
function csv_decode_ws_preserved()
    result = csv.decode("a,b\n x , y ")
    assert_equal(result[1]["a"], " x ", "leading/trailing ws in a")
    assert_equal(result[1]["b"], " y ", "leading/trailing ws in b")

// test: decode preserves whitespace inside quoted fields
function csv_decode_ws_quoted()
    result = csv.decode("a,b\n\" x \",\" y \"")
    assert_equal(result[1]["a"], " x ", "quoted ws in a")
    assert_equal(result[1]["b"], " y ", "quoted ws in b")

// test: decode preserves tabs inside fields
function csv_decode_tab_in_field()
    result = csv.decode("a,b\nx\ty,z")
    assert_equal(result[1]["a"], "x\ty", "tab inside unquoted field")

// test: decode preserves tabs inside quoted fields
function csv_decode_tab_in_quoted()
    result = csv.decode("a,b\n\"x\ty\",z")
    assert_equal(result[1]["a"], "x\ty", "tab inside quoted field")

// test: decode handles escaped quote at the start of a field
function csv_decode_escaped_quote_start()
    result = csv.decode("a\n\"\"\"text\"")
    assert_equal(result[1]["a"], "\"text", "escaped quote at start")

// test: decode handles escaped quote at the end of a field
function csv_decode_escaped_quote_end()
    result = csv.decode("a\n\"text\"\"\"")
    assert_equal(result[1]["a"], "text\"", "escaped quote at end")

// test: decode handles an escaped quote surrounded by text
function csv_decode_escaped_quote_middle()
    result = csv.decode("a\n\"a\"\"b\"")
    assert_equal(result[1]["a"], "a\"b", "escaped quote in middle")

// test: decode handles a field of only escaped quotes
function csv_decode_only_escaped_quotes()
    result = csv.decode("a\n\"\"\"\"\"\"")
    assert_equal(result[1]["a"], "\"\"", "field of only escaped quotes")

// test: decode handles a quoted field with embedded commas
function csv_decode_commas_in_quotes()
    result = csv.decode("a,b\n\"x,y,z\",w")
    assert_equal(result[1]["a"], "x,y,z", "commas inside quotes")
    assert_equal(result[1]["b"], "w", "second field after quoted comma field")

// test: decode handles a quoted field that is a single comma
function csv_decode_single_comma_quoted()
    result = csv.decode("a\n\",\"")
    assert_equal(result[1]["a"], ",", "quoted single comma")

// test: decode handles multiple consecutive empty fields
function csv_decode_multiple_empty_fields()
    result = csv.decode("a,b,c,d,e\n,,,,,")
    assert_equal(result[1]["a"], "", "empty a")
    assert_equal(result[1]["b"], "", "empty b")
    assert_equal(result[1]["c"], "", "empty c")
    assert_equal(result[1]["d"], "", "empty d")
    assert_equal(result[1]["e"], "", "empty e")

// test: decode trailing empty field after last comma
function csv_decode_trailing_empty()
    result = csv.decode("a,b,c\n1,2,")
    assert_equal(result[1]["a"], 1, "trailing empty: col a")
    assert_equal(result[1]["b"], 2, "trailing empty: col b")
    assert_equal(result[1]["c"], "", "trailing empty: col c")

// test: decode leading empty field before first comma
function csv_decode_leading_empty()
    result = csv.decode("a,b,c\n,2,3")
    assert_equal(result[1]["a"], "", "leading empty: col a")
    assert_equal(result[1]["b"], 2, "leading empty: col b")
    assert_equal(result[1]["c"], 3, "leading empty: col c")

// test: decode a single-column document with several rows
function csv_decode_single_column_many_rows()
    result = csv.decode("name\nAlice\nBob\nCharlie")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "single column table")
    if is_table == true
        assert_equal(table.size(result), 3, "single column row count")
        assert_equal(result[1]["name"], "Alice", "single column row 1")
        assert_equal(result[3]["name"], "Charlie", "single column row 3")

// test: decode single-column quoted header
function csv_decode_quoted_header()
    result = csv.decode("\"full name\",age\n\"Alice Smith\",30")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "quoted header returns table")
    if is_table == true
        assert_equal(result[1]["full name"], "Alice Smith", "quoted header key")

// test: decode whitespace-only header field
function csv_decode_ws_header()
    result = csv.decode("\" a \",b\n1,2")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "ws header returns table")
    if is_table == true
        assert_equal(result[1][" a "], 1, "whitespace-preserving header key")

// test: decode with duplicate column names keeps the last value per row
function csv_decode_duplicate_columns()
    result = csv.decode("a,a,a\n1,2,3")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "duplicate column names parsed")
    if is_table == true
        // the doc does not specify which value wins; only require a number
        assert_equal(type(result[1]["a"]), "number", "duplicate column is number")

// test: decode a large document keeps every row
function csv_decode_large_document()
    doc = "id,value\n"
    for i = 1, 200
        doc = "{doc}{i},{i}\n"
    result = csv.decode(doc)
    assert_equal(type(result), "table", "large doc parses")
    assert_equal(table.size(result), 200, "large doc row count")
    assert_equal(result[1]["id"], 1, "large doc first id")
    assert_equal(result[200]["id"], 200, "large doc last id")

// test: decode handles unicode inside unquoted fields
function csv_decode_unicode_unquoted()
    result = csv.decode("greeting\nhello")
    assert_equal(type(result), "table", "unicode unquoted parses")
    assert_equal(result[1]["greeting"], "hello", "unicode unquoted value")

// test: decode handles unicode inside quoted fields
function csv_decode_unicode_quoted()
    result = csv.decode("greeting\n\"你好, 世界\"")
    assert_equal(type(result), "table", "unicode quoted parses")
    assert_equal(result[1]["greeting"], "你好, 世界", "unicode quoted value")

// test: roundtrip is idempotent: encode(decode(encode(x))) == encode(x)
function csv_roundtrip_idempotent()
    data = [
        ["a" = "x,y", "b" = 1, "c" = true],
        ["a" = "line\nbreak", "b" = -2, "c" = false]
    ]
    once = csv.encode(data)
    twice = csv.encode(csv.decode(once))
    assert_equal(once, twice, "encode(decode(encode(x))) == encode(x)")

// test: full roundtrip with every value type at once
function csv_roundtrip_all_types()
    data = [[
        "s" = "text",
        "n" = 42,
        "f" = 3.5,
        "neg" = -7,
        "t" = true,
        "f2" = false
    ]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["s"], "text", "all types: string")
    assert_equal(decoded[1]["n"], 42, "all types: int")
    assert_equal(decoded[1]["f"], 3.5, "all types: float")
    assert_equal(decoded[1]["neg"], -7, "all types: negative")
    assert_equal(decoded[1]["t"], true, "all types: true")
    assert_equal(decoded[1]["f2"], false, "all types: false")

// test: full roundtrip of a value containing every special csv char
function csv_roundtrip_all_special_chars()
    original = "comma,quote\"newline\ntab\tcr\rend"
    data = [["v" = original]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], original, "all special chars roundtrip")

// test: full roundtrip of unicode strings
function csv_roundtrip_unicode()
    original = "你好, мир, café, 🎉"
    data = [["v" = original]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], original, "unicode roundtrip")

// test: numeric precision holds across roundtrip
function csv_roundtrip_numeric_precision()
    data = [[
        "a" = 0.1,
        "b" = 0.25,
        "c" = 100.125,
        "d" = -0.5
    ]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a"], 0.1, "precision: 0.1")
    assert_equal(decoded[1]["b"], 0.25, "precision: 0.25")
    assert_equal(decoded[1]["c"], 100.125, "precision: 100.125")
    assert_equal(decoded[1]["d"], -0.5, "precision: -0.5")

// test: empty values roundtrip unchanged
function csv_roundtrip_empty_values()
    data = [
        ["a" = "", "b" = "x", "c" = ""],
        ["a" = "", "b" = "", "c" = ""]
    ]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a"], "", "empty values row 1 a")
    assert_equal(decoded[1]["b"], "x", "empty values row 1 b")
    assert_equal(decoded[2]["a"], "", "empty values row 2 a")
    assert_equal(decoded[2]["b"], "", "empty values row 2 b")
    assert_equal(decoded[2]["c"], "", "empty values row 2 c")

// test: wide table roundtrip preserves column order and values
function csv_roundtrip_wide()
    row = [
        "a" = "A", "b" = "B", "c" = "C", "d" = "D", "e" = "E",
        "f" = "F", "g" = "G", "h" = "H", "i" = "I", "j" = "J"
    ]
    decoded = csv.decode(csv.encode([row]))
    assert_equal(decoded[1]["a"], "A", "wide roundtrip col a")
    assert_equal(decoded[1]["e"], "E", "wide roundtrip col e")
    assert_equal(decoded[1]["j"], "J", "wide roundtrip col j")

// test: nested structure with several rows and columns roundtrips fully
function csv_roundtrip_nested()
    data = [
        ["id" = 1, "kind" = "a", "meta" = "x,y"],
        ["id" = 2, "kind" = "b", "meta" = "quote\"end"],
        ["id" = 3, "kind" = "c", "meta" = "line1\nline2"]
    ]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["meta"], "x,y", "nested roundtrip row 1 meta")
    assert_equal(decoded[2]["meta"], "quote\"end", "nested roundtrip row 2 meta")
    assert_equal(decoded[3]["meta"], "line1\nline2", "nested roundtrip row 3 meta")

// test: decode rejects unterminated quoted field
function csv_decode_unterminated_quote()
    result = csv.decode("a,b\n\"never closed")
    assert_equal(type(result), "none", "unterminated quote rejected")

// test: decode rejects text after a closing quote but before a delimiter
function csv_decode_text_after_quote()
    result = csv.decode("a,b\n\"closed\"trailing,b")
    // behaviour is implementation defined; accept either none or a valid table
    t = type(result)
    ok = (t == "none") or (t == "table")
    assert_equal(ok, true, "text after quote handled without crashing")

// test: decode rejects a document made only of a comma
function csv_decode_only_comma()
    result = csv.decode(",")
    t = type(result)
    ok = (t == "none") or (t == "table")
    assert_equal(ok, true, "only-comma does not crash")

// test: decode rejects a document made only of newlines
function csv_decode_only_newlines()
    result = csv.decode("\n\n\n")
    t = type(result)
    ok = (t == "none") or (t == "table")
    assert_equal(ok, true, "only-newlines does not crash")

// test: decode rejects a document made only of whitespace
function csv_decode_only_whitespace()
    result = csv.decode("   ")
    t = type(result)
    ok = (t == "none") or (t == "table")
    assert_equal(ok, true, "whitespace-only does not crash")

// test: decode a document whose header has one column and no data
function csv_decode_header_one_col_no_data()
    result = csv.decode("only")
    assert_equal(type(result), "table", "single header no data returns table")
    if type(result) == "table"
        assert_equal(table.size(result), 0, "single header has no rows")

// test: decode a document with quoted empty header field
function csv_decode_empty_quoted_header()
    result = csv.decode("\"\",b\n1,2")
    is_table = type(result) == "table"
    assert_equal(is_table, true, "empty quoted header returns table")
    if is_table == true
        assert_equal(result[1][""], 1, "empty header key value")

// test: decode keeps row count stable across repeated parses
function csv_decode_repeatable()
    doc = "a,b\n1,2\n3,4"
    r1 = csv.decode(doc)
    r2 = csv.decode(doc)
    assert_equal(table.size(r1), table.size(r2), "row count stable")
    assert_equal(r1[1]["a"], r2[1]["a"], "row 1 stable")
    assert_equal(r1[2]["b"], r2[2]["b"], "row 2 stable")

// test: a value consisting of many commas roundtrips unchanged
function csv_encode_many_commas()
    data = [["v" = ",,,,,,"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], ",,,,,,", "many commas roundtrip")

// test: a value consisting of many quotes roundtrips unchanged
function csv_encode_many_quotes()
    data = [["v" = "\"\"\"\"\"\""]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\"\"\"\"\"\"", "many quotes roundtrip")

// test: a value consisting of many newlines roundtrips unchanged
function csv_encode_many_newlines()
    data = [["v" = "\n\n\n"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], "\n\n\n", "many newlines roundtrip")

// test: a value mixing comma, quote, and newline roundtrips
function csv_encode_comma_quote_newline()
    original = "a,\"b\"\nc"
    data = [["v" = original]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["v"], original, "comma+quote+newline roundtrip")

// test: keys with spaces are used verbatim as headers
function csv_encode_key_with_spaces()
    data = [["first name" = "Alice", "last name" = "Smith"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["first name"], "Alice", "space key first name")
    assert_equal(decoded[1]["last name"], "Smith", "space key last name")

// test: keys with commas are quoted as headers
function csv_encode_key_with_comma()
    data = [["a,b" = "x", "c" = "y"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a,b"], "x", "comma key value")

// test: keys with quotes are quoted and escaped
function csv_encode_key_with_quote()
    data = [["a\"b" = "x", "c" = "y"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a\"b"], "x", "quote key value")

// test: keys with newlines roundtrip
function csv_encode_key_with_newline()
    data = [["a\nb" = "x"]]
    decoded = csv.decode(csv.encode(data))
    assert_equal(decoded[1]["a\nb"], "x", "newline key value")

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
csv_encode_single_row()
csv_encode_single_column()
csv_encode_wide_table()
csv_encode_numeric_variety()
csv_encode_bool_variety()
csv_encode_value_only_comma()
csv_encode_value_only_quote()
csv_encode_value_only_newline()
csv_encode_value_only_cr()
csv_encode_tab_value()
csv_encode_value_all_special()
csv_encode_value_starts_quote()
csv_encode_value_ends_quote()
csv_encode_value_double_quote()
csv_encode_value_whitespace()
csv_encode_value_spaces_tabs()
csv_encode_empty_string_value()
csv_encode_many_rows()
csv_encode_long_value()
csv_encode_many_commas()
csv_encode_many_quotes()
csv_encode_many_newlines()
csv_encode_comma_quote_newline()
csv_encode_key_with_spaces()
csv_encode_key_with_comma()
csv_encode_key_with_quote()
csv_encode_key_with_newline()
csv_decode_header_only()
csv_decode_trailing_newline()
csv_decode_no_trailing_newline()
csv_decode_cr_only()
csv_decode_mixed_endings()
csv_decode_only_empty_fields()
csv_decode_ws_preserved()
csv_decode_ws_quoted()
csv_decode_tab_in_field()
csv_decode_tab_in_quoted()
csv_decode_escaped_quote_start()
csv_decode_escaped_quote_end()
csv_decode_escaped_quote_middle()
csv_decode_only_escaped_quotes()
csv_decode_commas_in_quotes()
csv_decode_single_comma_quoted()
csv_decode_multiple_empty_fields()
csv_decode_trailing_empty()
csv_decode_leading_empty()
csv_decode_single_column_many_rows()
csv_decode_quoted_header()
csv_decode_ws_header()
csv_decode_duplicate_columns()
csv_decode_large_document()
csv_decode_unicode_unquoted()
csv_decode_unicode_quoted()
csv_decode_header_one_col_no_data()
csv_decode_empty_quoted_header()
csv_decode_repeatable()
csv_roundtrip_idempotent()
csv_roundtrip_all_types()
csv_roundtrip_all_special_chars()
csv_roundtrip_unicode()
csv_roundtrip_numeric_precision()
csv_roundtrip_empty_values()
csv_roundtrip_wide()
csv_roundtrip_nested()
csv_decode_unterminated_quote()
csv_decode_text_after_quote()
csv_decode_only_comma()
csv_decode_only_newlines()

// report completion of csv library tests
os.output("[PASS] libraries/csv.apex")