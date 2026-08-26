// source/tests/libraries/hex.apex
// Implementation of Hex Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import hex
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: hex encoding handles UTF-8 text with multiple languages
function hex_roundtrip()
    chinese_text = "你好，世界！"
    hex_chinese = hex.encode(chinese_text)
    decoded_chinese = hex.decode(hex_chinese)
    assert_equal(decoded_chinese, chinese_text, "hex UTF-8 Chinese roundtrip")
    
    japanese_text = "こんにちは世界"
    hex_japanese = hex.encode(japanese_text)
    decoded_japanese = hex.decode(hex_japanese)
    assert_equal(decoded_japanese, japanese_text, "hex UTF-8 Japanese roundtrip")
    
    korean_text = "안녕하세요 세계"
    hex_korean = hex.encode(korean_text)
    decoded_korean = hex.decode(hex_korean)
    assert_equal(decoded_korean, korean_text, "hex UTF-8 Korean roundtrip")
    
    russian_text = "Привет, мир!"
    hex_russian = hex.encode(russian_text)
    decoded_russian = hex.decode(hex_russian)
    assert_equal(decoded_russian, russian_text, "hex UTF-8 Russian roundtrip")
    
    arabic_text = "مرحبا بالعالم"
    hex_arabic = hex.encode(arabic_text)
    decoded_arabic = hex.decode(hex_arabic)
    assert_equal(decoded_arabic, arabic_text, "hex UTF-8 Arabic roundtrip")
    
    hebrew_text = "שלום עולם"
    hex_hebrew = hex.encode(hebrew_text)
    decoded_hebrew = hex.decode(hex_hebrew)
    assert_equal(decoded_hebrew, hebrew_text, "hex UTF-8 Hebrew roundtrip")
    
    greek_text = "Γειά σου Κόσμε"
    hex_greek = hex.encode(greek_text)
    decoded_greek = hex.decode(hex_greek)
    assert_equal(decoded_greek, greek_text, "hex UTF-8 Greek roundtrip")
    
    thai_text = "สวัสดีชาวโลก"
    hex_thai = hex.encode(thai_text)
    decoded_thai = hex.decode(hex_thai)
    assert_equal(decoded_thai, thai_text, "hex UTF-8 Thai roundtrip")
    
    hindi_text = "नमस्ते दुनिया"
    hex_hindi = hex.encode(hindi_text)
    decoded_hindi = hex.decode(hex_hindi)
    assert_equal(decoded_hindi, hindi_text, "hex UTF-8 Hindi roundtrip")
    
    vietnamese_text = "Xin chào thế giới"
    hex_vietnamese = hex.encode(vietnamese_text)
    decoded_vietnamese = hex.decode(hex_vietnamese)
    assert_equal(decoded_vietnamese, vietnamese_text, "hex UTF-8 Vietnamese roundtrip")
    
    emoji_text = "Hello 🌍🚀💻"
    hex_emoji = hex.encode(emoji_text)
    decoded_emoji = hex.decode(hex_emoji)
    assert_equal(decoded_emoji, emoji_text, "hex UTF-8 emoji roundtrip")
    
    mixed_text = "Hello 世界 안녕하세요 Привет مرحبا"
    hex_mixed = hex.encode(mixed_text)
    decoded_mixed = hex.decode(hex_mixed)
    assert_equal(decoded_mixed, mixed_text, "hex UTF-8 mixed languages roundtrip")

// test: hex encoding handles binary data
function hex_binary_data()
    binary_data = hex.decode("00010203FFFEFD")
    hex_result = hex.encode(binary_data)
    decoded_result = hex.decode(hex_result)
    assert_equal(decoded_result, binary_data, "hex binary data roundtrip")
    assert_equal(hex_result, "00010203fffefd", "hex binary encoding")

// test: hex read is case insensitive
function hex_case_insensitive()
    lower_input = "48656c6c6f"
    upper_input = "48656C6C6F"
    mixed_input = "48656c6C6f"
    
    decoded_lower = hex.decode(lower_input)
    decoded_upper = hex.decode(upper_input)
    decoded_mixed = hex.decode(mixed_input)
    
    assert_equal(decoded_lower, "Hello", "hex lowercase decode")
    assert_equal(decoded_upper, "Hello", "hex uppercase decode")
    assert_equal(decoded_mixed, "Hello", "hex mixed case decode")

// test: hex read handles spaces and formatting
function hex_with_spaces()
    spaced_hex = "48 65 6c 6c 6f"
    newline_hex = "48\n65\t6c\r6c6f"
    
    decoded_spaces = hex.decode(spaced_hex)
    decoded_newlines = hex.decode(newline_hex)
    
    assert_equal(decoded_spaces, "Hello", "hex decode with spaces")
    assert_equal(decoded_newlines, "Hello", "hex decode with whitespace")

// test: hex read handles odd number of hex characters
function hex_odd_characters()
    odd_input = "48656c6c6f2"  // missing last nibble
    result = hex.decode(odd_input)
    assert_equal(type(result), "string", "hex read odd chars returns string")
    
    single_input = "4"
    result2 = hex.decode(single_input)
    assert_equal(type(result2), "string", "hex read single char returns string")

// test: hex read handles empty input
function hex_empty_input()
    empty_str = ""
    result = hex.decode(empty_str)
    assert_equal(result, "", "hex read empty string returns empty")
    
    whitespace_only = "   \t\n  "
    result2 = hex.decode(whitespace_only)
    assert_equal(result2, "", "hex read only whitespace returns empty")

// test: hex read handles invalid characters
function hex_invalid_characters()
    invalid_input = "48GG65"  // G is invalid hex
    result = hex.decode(invalid_input)
    assert_equal(type(result), "string", "hex read invalid chars returns string")
    assert_equal(result, "He", "hex read skips invalid chars")

// execute all hex library test cases
hex_roundtrip()
hex_binary_data()
hex_case_insensitive()
hex_with_spaces()
hex_odd_characters()
hex_empty_input()
hex_invalid_characters()

// report completion of hex library tests
os.output("[PASS] libraries/hex.apex")