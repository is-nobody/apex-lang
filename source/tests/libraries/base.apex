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

// test: base64 encoding and decoding roundtrip
function base64_roundtrip()
    text = "Hello, World! This is a test of Base64."
    b64_str = base.encode(text)
    decoded_b64 = base.decode(b64_str)
    assert_equal(decoded_b64, text, "base64 roundtrip")

// test: base64url encoding and decoding roundtrip
function base64url_roundtrip()
    url_data = "data?param=value&other=123"
    b64url_str = base.url_encode(url_data)
    decoded_b64url = base.url_decode(b64url_str)
    assert_equal(decoded_b64url, url_data, "base64url roundtrip")

// test: base64_decode rejects invalid base64 characters
function base64_invalid_input()
    invalid_b64 = "ABC!DEF"
    result = base.decode(invalid_b64)
    assert_equal(type(result), "none", "base64_decode invalid char returns none")
    
    invalid_b64_2 = "ABCD="
    result2 = base.decode(invalid_b64_2)
    assert_equal(type(result2), "none", "base64_decode wrong padding returns none")

// test: base64url_decode rejects invalid base64url characters
function base64url_invalid_input()
    invalid_b64url = "ABC!DEF"
    result = base.url_decode(invalid_b64url)
    assert_equal(type(result), "none", "base64url_decode invalid char returns none")

// execute all base library test cases
base64_roundtrip()
base64url_roundtrip()
base64_invalid_input()
base64url_invalid_input()

// report completion of base library tests
os.output("[PASS] libraries/base.apex")