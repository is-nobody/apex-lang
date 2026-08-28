// source/tests/libraries/base.apex
// Implementation of Base Library tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import base
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: base64 encoding/decoding
function base64_known_values()
    // base64 encoding
    encoded1 = base.encode_64("")
    assert_equal(encoded1, "", "base64 encode empty string")
    
    encoded2 = base.encode_64("f")
    assert_equal(encoded2, "Zg==", "base64 encode 'f'")
    
    encoded3 = base.encode_64("fo")
    assert_equal(encoded3, "Zm8=", "base64 encode 'fo'")
    
    encoded4 = base.encode_64("foo")
    assert_equal(encoded4, "Zm9v", "base64 encode 'foo'")
    
    encoded5 = base.encode_64("foob")
    assert_equal(encoded5, "Zm9vYg==", "base64 encode 'foob'")
    
    encoded6 = base.encode_64("fooba")
    assert_equal(encoded6, "Zm9vYmE=", "base64 encode 'fooba'")
    
    encoded7 = base.encode_64("foobar")
    assert_equal(encoded7, "Zm9vYmFy", "base64 encode 'foobar'")
    
    // base64 decoding
    decoded1 = base.decode_64("")
    assert_equal(decoded1, "", "base64 decode empty string")
    
    decoded2 = base.decode_64("Zg==")
    assert_equal(decoded2, "f", "base64 decode 'Zg=='")
    
    decoded3 = base.decode_64("Zm8=")
    assert_equal(decoded3, "fo", "base64 decode 'Zm8='")
    
    decoded4 = base.decode_64("Zm9v")
    assert_equal(decoded4, "foo", "base64 decode 'Zm9v'")
    
    decoded5 = base.decode_64("Zm9vYg==")
    assert_equal(decoded5, "foob", "base64 decode 'Zm9vYg=='")
    
    decoded6 = base.decode_64("Zm9vYmE=")
    assert_equal(decoded6, "fooba", "base64 decode 'Zm9vYmE='")
    
    decoded7 = base.decode_64("Zm9vYmFy")
    assert_equal(decoded7, "foobar", "base64 decode 'Zm9vYmFy'")

// test: base64url encoding/decoding
function base64url_known_values()
    // base64url encoding
    encoded1 = base.encode_64url("")
    assert_equal(encoded1, "", "base64url encode empty string")
    
    encoded2 = base.encode_64url("f")
    assert_equal(encoded2, "Zg", "base64url encode 'f'")
    
    encoded3 = base.encode_64url("fo")
    assert_equal(encoded3, "Zm8", "base64url encode 'fo'")
    
    encoded4 = base.encode_64url("foo")
    assert_equal(encoded4, "Zm9v", "base64url encode 'foo'")
    
    encoded5 = base.encode_64url("foob")
    assert_equal(encoded5, "Zm9vYg", "base64url encode 'foob'")
    
    encoded6 = base.encode_64url("fooba")
    assert_equal(encoded6, "Zm9vYmE", "base64url encode 'fooba'")
    
    encoded7 = base.encode_64url("foobar")
    assert_equal(encoded7, "Zm9vYmFy", "base64url encode 'foobar'")
    
    // base64url decoding
    decoded1 = base.decode_64url("")
    assert_equal(decoded1, "", "base64url decode empty string")
    
    decoded2 = base.decode_64url("Zg")
    assert_equal(decoded2, "f", "base64url decode 'Zg'")
    
    decoded3 = base.decode_64url("Zm8")
    assert_equal(decoded3, "fo", "base64url decode 'Zm8'")
    
    decoded4 = base.decode_64url("Zm9v")
    assert_equal(decoded4, "foo", "base64url decode 'Zm9v'")
    
    decoded5 = base.decode_64url("Zm9vYg")
    assert_equal(decoded5, "foob", "base64url decode 'Zm9vYg'")
    
    decoded6 = base.decode_64url("Zm9vYmE")
    assert_equal(decoded6, "fooba", "base64url decode 'Zm9vYmE'")
    
    decoded7 = base.decode_64url("Zm9vYmFy")
    assert_equal(decoded7, "foobar", "base64url decode 'Zm9vYmFy'")

// test: base16 encoding/decoding
function base16_known_values()
    // base16 encoding
    encoded1 = base.encode_16("")
    assert_equal(encoded1, "", "base16 encode empty string")
    
    encoded2 = base.encode_16("f")
    assert_equal(encoded2, "66", "base16 encode 'f'")
    
    encoded3 = base.encode_16("fo")
    assert_equal(encoded3, "666F", "base16 encode 'fo'")
    
    encoded4 = base.encode_16("foo")
    assert_equal(encoded4, "666F6F", "base16 encode 'foo'")
    
    encoded5 = base.encode_16("foob")
    assert_equal(encoded5, "666F6F62", "base16 encode 'foob'")
    
    encoded6 = base.encode_16("fooba")
    assert_equal(encoded6, "666F6F6261", "base16 encode 'fooba'")
    
    encoded7 = base.encode_16("foobar")
    assert_equal(encoded7, "666F6F626172", "base16 encode 'foobar'")
    
    // base16 decoding
    decoded1 = base.decode_16("")
    assert_equal(decoded1, "", "base16 decode empty string")
    
    decoded2 = base.decode_16("66")
    assert_equal(decoded2, "f", "base16 decode '66'")
    
    decoded3 = base.decode_16("666F")
    assert_equal(decoded3, "fo", "base16 decode '666F'")
    
    decoded4 = base.decode_16("666F6F")
    assert_equal(decoded4, "foo", "base16 decode '666F6F'")
    
    decoded5 = base.decode_16("666F6F62")
    assert_equal(decoded5, "foob", "base16 decode '666F6F62'")
    
    decoded6 = base.decode_16("666F6F6261")
    assert_equal(decoded6, "fooba", "base16 decode '666F6F6261'")
    
    decoded7 = base.decode_16("666F6F626172")
    assert_equal(decoded7, "foobar", "base16 decode '666F6F626172'")
    
    // lowercase hex decoding
    decoded_lower = base.decode_16("666f6f626172")
    assert_equal(decoded_lower, "foobar", "base16 decode lowercase hex")
    
    // mixed case hex decoding
    decoded_mixed = base.decode_16("666F6f626172")
    assert_equal(decoded_mixed, "foobar", "base16 decode mixed case hex")

// test: base16 encoding handles utf-8 text with multiple languages
function base16_utf8_roundtrip()
    chinese_text = "你好，世界！"
    hex_chinese = base.encode_16(chinese_text)
    decoded_chinese = base.decode_16(hex_chinese)
    assert_equal(decoded_chinese, chinese_text, "base16 UTF-8 Chinese roundtrip")
    
    japanese_text = "こんにちは世界"
    hex_japanese = base.encode_16(japanese_text)
    decoded_japanese = base.decode_16(hex_japanese)
    assert_equal(decoded_japanese, japanese_text, "base16 UTF-8 Japanese roundtrip")
    
    korean_text = "안녕하세요 세계"
    hex_korean = base.encode_16(korean_text)
    decoded_korean = base.decode_16(hex_korean)
    assert_equal(decoded_korean, korean_text, "base16 UTF-8 Korean roundtrip")
    
    russian_text = "Привет, мир!"
    hex_russian = base.encode_16(russian_text)
    decoded_russian = base.decode_16(hex_russian)
    assert_equal(decoded_russian, russian_text, "base16 UTF-8 Russian roundtrip")
    
    arabic_text = "مرحبا بالعالم"
    hex_arabic = base.encode_16(arabic_text)
    decoded_arabic = base.decode_16(hex_arabic)
    assert_equal(decoded_arabic, arabic_text, "base16 UTF-8 Arabic roundtrip")
    
    hebrew_text = "שלום עולם"
    hex_hebrew = base.encode_16(hebrew_text)
    decoded_hebrew = base.decode_16(hex_hebrew)
    assert_equal(decoded_hebrew, hebrew_text, "base16 UTF-8 Hebrew roundtrip")
    
    greek_text = "Γειά σου Κόσμε"
    hex_greek = base.encode_16(greek_text)
    decoded_greek = base.decode_16(hex_greek)
    assert_equal(decoded_greek, greek_text, "base16 UTF-8 Greek roundtrip")
    
    thai_text = "สวัสดีชาวโลก"
    hex_thai = base.encode_16(thai_text)
    decoded_thai = base.decode_16(hex_thai)
    assert_equal(decoded_thai, thai_text, "base16 UTF-8 Thai roundtrip")
    
    hindi_text = "नमस्ते दुनिया"
    hex_hindi = base.encode_16(hindi_text)
    decoded_hindi = base.decode_16(hex_hindi)
    assert_equal(decoded_hindi, hindi_text, "base16 UTF-8 Hindi roundtrip")
    
    vietnamese_text = "Xin chào thế giới"
    hex_vietnamese = base.encode_16(vietnamese_text)
    decoded_vietnamese = base.decode_16(hex_vietnamese)
    assert_equal(decoded_vietnamese, vietnamese_text, "base16 UTF-8 Vietnamese roundtrip")
    
    emoji_text = "Hello 🌍🚀💻"
    hex_emoji = base.encode_16(emoji_text)
    decoded_emoji = base.decode_16(hex_emoji)
    assert_equal(decoded_emoji, emoji_text, "base16 UTF-8 emoji roundtrip")
    
    mixed_text = "Hello 世界 안녕하세요 Привет مرحبا"
    hex_mixed = base.encode_16(mixed_text)
    decoded_mixed = base.decode_16(hex_mixed)
    assert_equal(decoded_mixed, mixed_text, "base16 UTF-8 mixed languages roundtrip")

// test: base16 encoding handles binary data
function base16_binary_data()
    binary_data = base.decode_16("00010203FFFEFD")
    hex_result = base.encode_16(binary_data)
    decoded_result = base.decode_16(hex_result)
    assert_equal(decoded_result, binary_data, "base16 binary data roundtrip")
    assert_equal(hex_result, "00010203FFFEFD", "base16 binary encoding")

// test: base16 decode is case insensitive
function base16_case_insensitive()
    lower_input = "48656c6c6f"
    upper_input = "48656C6C6F"
    mixed_input = "48656c6C6f"
    
    decoded_lower = base.decode_16(lower_input)
    decoded_upper = base.decode_16(upper_input)
    decoded_mixed = base.decode_16(mixed_input)
    
    assert_equal(decoded_lower, "Hello", "base16 lowercase decode")
    assert_equal(decoded_upper, "Hello", "base16 uppercase decode")
    assert_equal(decoded_mixed, "Hello", "base16 mixed case decode")

// test: base16 decode handles spaces and formatting
function base16_with_spaces()
    spaced_hex = "48 65 6c 6c 6f"
    newline_hex = "48\n65\t6c\r6c6f"
    
    decoded_spaces = base.decode_16(spaced_hex)
    decoded_newlines = base.decode_16(newline_hex)
    
    assert_equal(type(decoded_spaces), "none", "base16 decode with spaces returns none")
    assert_equal(type(decoded_newlines), "none", "base16 decode with whitespace returns none")

// test: base16 decode handles empty input
function base16_empty_input()
    empty_str = ""
    result = base.decode_16(empty_str)
    assert_equal(result, "", "base16 decode empty string returns empty")
    
    whitespace_only = "   \t\n  "
    result2 = base.decode_16(whitespace_only)
    assert_equal(type(result2), "none", "base16 decode only whitespace returns none")

// test: base16 decode handles odd number of hex characters
function base16_odd_characters()
    odd_input = "48656c6c6f2"  // missing last nibble
    result = base.decode_16(odd_input)
    assert_equal(type(result), "none", "base16 decode odd chars returns none")
    
    single_input = "4"
    result2 = base.decode_16(single_input)
    assert_equal(type(result2), "none", "base16 decode single char returns none")

// test: base16 decode handles empty input
function base16_empty_input()
    empty_str = ""
    result = base.decode_16(empty_str)
    assert_equal(result, "", "base16 decode empty string returns empty")
    
    whitespace_only = "   \t\n  "
    result2 = base.decode_16(whitespace_only)
    assert_equal(result2, "", "base16 decode only whitespace returns empty")

// test: base16 decode rejects invalid characters
function base16_invalid_characters()
    invalid_input = "48GG65"  // G is invalid hex
    result = base.decode_16(invalid_input)
    assert_equal(type(result), "none", "base16 decode invalid chars returns none")
    
    invalid_hex = "12G4"
    result2 = base.decode_16(invalid_hex)
    assert_equal(type(result2), "none", "base16 decode invalid char G returns none")
    
    invalid_hex_odd = "123"
    result3 = base.decode_16(invalid_hex_odd)
    assert_equal(type(result3), "none", "base16 decode odd length returns none")

// test: base32 encoding/decoding
function base32_known_values()
    // base32 encoding
    encoded1 = base.encode_32("")
    assert_equal(encoded1, "", "base32 encode empty string")
    
    encoded2 = base.encode_32("f")
    assert_equal(encoded2, "MY======", "base32 encode 'f'")
    
    encoded3 = base.encode_32("fo")
    assert_equal(encoded3, "MZXQ====", "base32 encode 'fo'")
    
    encoded4 = base.encode_32("foo")
    assert_equal(encoded4, "MZXW6===", "base32 encode 'foo'")
    
    encoded5 = base.encode_32("foob")
    assert_equal(encoded5, "MZXW6YQ=", "base32 encode 'foob'")
    
    encoded6 = base.encode_32("fooba")
    assert_equal(encoded6, "MZXW6YTB", "base32 encode 'fooba'")
    
    encoded7 = base.encode_32("foobar")
    assert_equal(encoded7, "MZXW6YTBOI======", "base32 encode 'foobar'")
    
    // base32 decoding
    decoded1 = base.decode_32("")
    assert_equal(decoded1, "", "base32 decode empty string")
    
    decoded2 = base.decode_32("MY======")
    assert_equal(decoded2, "f", "base32 decode 'MY======'")
    
    decoded3 = base.decode_32("MZXQ====")
    assert_equal(decoded3, "fo", "base32 decode 'MZXQ===='")
    
    decoded4 = base.decode_32("MZXW6===")
    assert_equal(decoded4, "foo", "base32 decode 'MZXW6==='")
    
    decoded5 = base.decode_32("MZXW6YQ=")
    assert_equal(decoded5, "foob", "base32 decode 'MZXW6YQ='")
    
    decoded6 = base.decode_32("MZXW6YTB")
    assert_equal(decoded6, "fooba", "base32 decode 'MZXW6YTB'")
    
    decoded7 = base.decode_32("MZXW6YTBOI======")
    assert_equal(decoded7, "foobar", "base32 decode 'MZXW6YTBOI======'")
    
    // test lowercase decoding
    decoded_lower = base.decode_32("mzxw6ytboi======")
    assert_equal(decoded_lower, "foobar", "base32 decode lowercase")

// test: base32hex encoding/decoding
function base32hex_known_values()
    // base32hex encoding
    encoded1 = base.encode_32hex("")
    assert_equal(encoded1, "", "base32hex encode empty string")
    
    encoded2 = base.encode_32hex("f")
    assert_equal(encoded2, "CO======", "base32hex encode 'f'")
    
    encoded3 = base.encode_32hex("fo")
    assert_equal(encoded3, "CPNG====", "base32hex encode 'fo'")
    
    encoded4 = base.encode_32hex("foo")
    assert_equal(encoded4, "CPNMU===", "base32hex encode 'foo'")
    
    encoded5 = base.encode_32hex("foob")
    assert_equal(encoded5, "CPNMUOG=", "base32hex encode 'foob'")
    
    encoded6 = base.encode_32hex("fooba")
    assert_equal(encoded6, "CPNMUOJ1", "base32hex encode 'fooba'")
    
    encoded7 = base.encode_32hex("foobar")
    assert_equal(encoded7, "CPNMUOJ1E8======", "base32hex encode 'foobar'")
    
    // base32hex decoding
    decoded1 = base.decode_32hex("")
    assert_equal(decoded1, "", "base32hex decode empty string")
    
    decoded2 = base.decode_32hex("CO======")
    assert_equal(decoded2, "f", "base32hex decode 'CO======'")
    
    decoded3 = base.decode_32hex("CPNG====")
    assert_equal(decoded3, "fo", "base32hex decode 'CPNG===='")
    
    decoded4 = base.decode_32hex("CPNMU===")
    assert_equal(decoded4, "foo", "base32hex decode 'CPNMU==='")
    
    decoded5 = base.decode_32hex("CPNMUOG=")
    assert_equal(decoded5, "foob", "base32hex decode 'CPNMUOG='")
    
    decoded6 = base.decode_32hex("CPNMUOJ1")
    assert_equal(decoded6, "fooba", "base32hex decode 'CPNMUOJ1'")
    
    decoded7 = base.decode_32hex("CPNMUOJ1E8======")
    assert_equal(decoded7, "foobar", "base32hex decode 'CPNMUOJ1E8======'")
    
    // test lowercase decoding
    decoded_lower = base.decode_32hex("cpnmuoj1e8======")
    assert_equal(decoded_lower, "foobar", "base32hex decode lowercase")

// test: base62 encoding/decoding
function base62_known_values()
    // base62 encoding
    encoded1 = base.encode_62("")
    assert_equal(encoded1, "", "base62 encode empty string")
    
    encoded2 = base.encode_62("f")
    assert_equal(encoded2, "1e", "base62 encode 'f'")
    
    encoded3 = base.encode_62("fo")
    assert_equal(encoded3, "6ox", "base62 encode 'fo'")
    
    encoded4 = base.encode_62("foo")
    assert_equal(encoded4, "SAPP", "base62 encode 'foo'")
    
    encoded5 = base.encode_62("foob")
    assert_equal(encoded5, "1sIyuo", "base62 encode 'foob'")
    
    encoded6 = base.encode_62("fooba")
    assert_equal(encoded6, "7kENWa1", "base62 encode 'fooba'")
    
    encoded7 = base.encode_62("foobar")
    assert_equal(encoded7, "VytN8Wjy", "base62 encode 'foobar'")
    
    // base62 decoding
    decoded1 = base.decode_62("")
    assert_equal(decoded1, "", "base62 decode empty string")
    
    decoded2 = base.decode_62("1e")
    assert_equal(decoded2, "f", "base62 decode '1e'")
    
    decoded3 = base.decode_62("6ox")
    assert_equal(decoded3, "fo", "base62 decode '6ox'")
    
    decoded4 = base.decode_62("SAPP")
    assert_equal(decoded4, "foo", "base62 decode 'SAPP'")
    
    decoded5 = base.decode_62("1sIyuo")
    assert_equal(decoded5, "foob", "base62 decode '1sIyuo'")
    
    decoded6 = base.decode_62("7kENWa1")
    assert_equal(decoded6, "fooba", "base62 decode '7kENWa1'")
    
    decoded7 = base.decode_62("VytN8Wjy")
    assert_equal(decoded7, "foobar", "base62 decode 'VytN8Wjy'")
    
    // test with numbers and special characters
    encoded8 = base.encode_62("12345")
    decoded8 = base.decode_62(encoded8)
    assert_equal(decoded8, "12345", "base62 roundtrip '12345'")
    
    encoded9 = base.encode_62("Hello, World!")
    decoded9 = base.decode_62(encoded9)
    assert_equal(decoded9, "Hello, World!", "base62 roundtrip 'Hello, World!'")
    
    // additional roundtrip tests
    test_strings = ["test", "abc123", "!@#$%", "The quick brown fox"]
    for s in test_strings
        encoded = base.encode_62(s)
        decoded = base.decode_62(encoded)
        assert_equal(decoded, s, "base62 roundtrip '{s}'")

// test: base85 encoding/decoding
function base85_known_values()
    // base85 encoding
    encoded1 = base.encode_85("")
    assert_equal(encoded1, "", "base85 encode empty string")
    
    encoded2 = base.encode_85("f")
    assert_equal(encoded2, "Ac", "base85 encode 'f'")
    
    encoded3 = base.encode_85("fo")
    assert_equal(encoded3, "Ao@", "base85 encode 'fo'")
    
    encoded4 = base.encode_85("foo")
    assert_equal(encoded4, "AoDS", "base85 encode 'foo'")
    
    encoded5 = base.encode_85("foob")
    assert_equal(encoded5, "AoDTs", "base85 encode 'foob'")
    
    encoded6 = base.encode_85("fooba")
    assert_equal(encoded6, "AoDTs@/", "base85 encode 'fooba'")
    
    encoded7 = base.encode_85("foobar")
    assert_equal(encoded7, "AoDTs@<)", "base85 encode 'foobar'")
    
    // base85 decoding
    decoded1 = base.decode_85("")
    assert_equal(decoded1, "", "base85 decode empty string")
    
    decoded2 = base.decode_85("Ac")
    assert_equal(decoded2, "f", "base85 decode 'Ac'")
    
    decoded3 = base.decode_85("Ao@")
    assert_equal(decoded3, "fo", "base85 decode 'Ao@'")
    
    decoded4 = base.decode_85("AoDS")
    assert_equal(decoded4, "foo", "base85 decode 'AoDS'")
    
    decoded5 = base.decode_85("AoDTs")
    assert_equal(decoded5, "foob", "base85 decode 'AoDTs'")
    
    decoded6 = base.decode_85("AoDTs@/")
    assert_equal(decoded6, "fooba", "base85 decode 'AoDTs@/'")
    
    decoded7 = base.decode_85("AoDTs@<)")
    assert_equal(decoded7, "foobar", "base85 decode 'AoDTs@<)'")
    
    // test roundtrip with printable text
    test_text = "Hello, World! This is a test."
    encoded_text = base.encode_85(test_text)
    decoded_text = base.decode_85(encoded_text)
    assert_equal(decoded_text, test_text, "base85 roundtrip printable text")

// test: base64_decode rejects invalid base64 characters
function base64_invalid_input()
    invalid_b64 = "ABC!DEF"
    result = base.decode_64(invalid_b64)
    assert_equal(type(result), "none", "base64_decode invalid char returns none")
    
    invalid_b64_2 = "ABCD="
    result2 = base.decode_64(invalid_b64_2)
    assert_equal(type(result2), "none", "base64_decode wrong padding returns none")

// test: base64url_decode rejects invalid base64url characters
function base64url_invalid_input()
    invalid_b64url = "ABC!DEF"
    result = base.decode_64url(invalid_b64url)
    assert_equal(type(result), "none", "base64url_decode invalid char returns none")

// test: base16_decode rejects invalid hex characters
function base16_invalid_input()
    invalid_hex = "12G4"
    result = base.decode_16(invalid_hex)
    assert_equal(type(result), "none", "base16_decode invalid char returns none")
    
    invalid_hex_odd = "123"
    result2 = base.decode_16(invalid_hex_odd)
    assert_equal(type(result2), "none", "base16_decode odd length returns none")

// test: base32_decode rejects invalid base32 characters
function base32_invalid_input()
    invalid_b32 = "ABC!DEF"
    result = base.decode_32(invalid_b32)
    assert_equal(type(result), "none", "base32_decode invalid char returns none")
    
    invalid_b32_2 = "ABC1DEF"
    result2 = base.decode_32(invalid_b32_2)
    assert_equal(type(result2), "none", "base32_decode invalid char 1 returns none")

// test: base32hex_decode rejects invalid base32hex characters
function base32hex_invalid_input()
    invalid_b32hex = "ABC!DEF"
    result = base.decode_32hex(invalid_b32hex)
    assert_equal(type(result), "none", "base32hex_decode invalid char returns none")
    
    invalid_b32hex_2 = "WXYZ1234"
    result2 = base.decode_32hex(invalid_b32hex_2)
    assert_equal(type(result2), "none", "base32hex_decode invalid char W returns none")

// test: base62_decode rejects invalid base62 characters
function base62_invalid_input()
    invalid_b62 = "ABC!DEF"
    result = base.decode_62(invalid_b62)
    assert_equal(type(result), "none", "base62_decode invalid char returns none")
    
    invalid_b62_2 = "ABC-DEF"
    result2 = base.decode_62(invalid_b62_2)
    assert_equal(type(result2), "none", "base62_decode invalid char '-' returns none")

// test: base85_decode rejects invalid ascii85 characters
function base85_invalid_input()
    invalid_b85 = "ABCvDEF"
    result = base.decode_85(invalid_b85)
    assert_equal(type(result), "none", "base85_decode invalid char 'v' returns none")
    
    invalid_b85_2 = "ABC DEF"
    result2 = base.decode_85(invalid_b85_2)
    assert_equal(type(result2), "none", "base85_decode invalid char space returns none")

// execute all base library test cases
base64_known_values()
base64url_known_values()
base16_known_values()
base16_utf8_roundtrip()
base16_binary_data()
base16_case_insensitive()
base16_with_spaces()
base16_odd_characters()
base16_empty_input()
base16_invalid_characters()
base32_known_values()
base32hex_known_values()
base62_known_values()
base85_known_values()
base64_invalid_input()
base64url_invalid_input()
base16_invalid_input()
base32_invalid_input()
base32hex_invalid_input()
base62_invalid_input()
base85_invalid_input()

// report completion of base library tests
os.output("[PASS] libraries/base.apex")