// source/tests/libraries/base.apex
// Implementation of Base Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import base
import table

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: base64 encoding with rfc 4648 test vectors
function base64_known_values()
    // rfc 4648 Section 10 test vectors
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
    
    // rfc 4648 decoding test vectors
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

// test: base64url encoding with rfc 4648 test vectors
function base64url_known_values()
    // rfc 4648 Section 10 test vectors for base64url
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
    
    // rfc 4648 decoding test vectors
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

// test: base16 (hex) encoding with rfc 4648 test vectors
function base16_known_values()
    // rfc 4648 Section 10 test vectors for base16
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
    
    // rfc 4648 decoding test vectors
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
    
    // Test lowercase hex decoding
    decoded_lower = base.decode_16("666f6f626172")
    assert_equal(decoded_lower, "foobar", "base16 decode lowercase hex")
    
    // Test mixed case hex decoding
    decoded_mixed = base.decode_16("666F6f626172")
    assert_equal(decoded_mixed, "foobar", "base16 decode mixed case hex")

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

// execute all base library test cases
base64_known_values()
base64url_known_values()
base16_known_values()
base64_invalid_input()
base64url_invalid_input()
base16_invalid_input()

// report completion of base library tests
os.output("[PASS] libraries/base.apex")