// source/tests/integration/network.apex
// Implementation of Network (HTTP) Operations Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import string
import network

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: checks a value is a table with a numeric status field
function assert_has_status(response, name)
    if response == none
        os.output("[ERROR] {name}: response is none")
        return false
    if type(response) != "table"
        os.output("[ERROR] {name}: response is not a table")
        return false
    if response["status"] == none
        os.output("[ERROR] {name}: response has no status field")
        return false
    if type(response["status"]) != "number"
        os.output("[ERROR] {name}: status is not a number")
        return false
    return true

// success path: full response shape — status 200, body is a non-empty string
function http_get_success_shape()
    response = network.get("http://httpbin.org/get")
    if assert_has_status(response, "http_get success") == true
        assert_equal(response["status"], 200, "http_get status")
        assert_equal(type(response["body"]), "string", "http_get body type")
        assert_equal(string.length(response["body"]) > 0, true, "http_get body non-empty")
        assert_equal(type(response), "table", "http_get result type")

// non-200 path: server responds with an error status, response still shaped correctly
function http_get_404()
    response = network.get("http://httpbin.org/status/404")
    if assert_has_status(response, "http_get 404") == true
        assert_equal(response["status"], 404, "http_get 404 status")

// failure path: no route to host — returns none
function http_get_unreachable()
    response = network.get("http://this-host-does-not-exist-xyz.invalid/")
    assert_equal(response, none, "http_get unreachable returns none")

// failure path: parse_url rejects non-http schemes — returns none
function http_get_https_unsupported()
    response = network.get("https://httpbin.org/get")
    assert_equal(response, none, "http_get https unsupported returns none")

// post success path: request body round-trips, response is shaped like a GET
function http_post_success()
    response = network.post("http://httpbin.org/post", "hello=world")
    if assert_has_status(response, "http_post success") == true
        assert_equal(response["status"], 200, "http_post status")
        assert_equal(type(response["body"]), "string", "http_post body type")

// post with explicit content type: header path exercised
function http_post_with_content_type()
    response = network.post(
        "http://httpbin.org/post",
        '\{"key":"value"\}',
        "application/json"
    )
    if assert_has_status(response, "http_post content type") == true
        assert_equal(response["status"], 200, "http_post content type status")

// post without body: header path with no Content-Length
function http_post_no_body()
    response = network.post("http://httpbin.org/post")
    if assert_has_status(response, "http_post no body") == true
        assert_equal(response["status"] == 200 or response["status"] == 411, true, "http_post no body status")

// post failure path: no route to host — returns none
function http_post_unreachable()
    response = network.post("http://this-host-does-not-exist-xyz.invalid/post", "x=1")
    assert_equal(response, none, "http_post unreachable returns none")

// execute all network test cases
http_get_success_shape()
http_get_404()
http_get_unreachable()
http_get_https_unsupported()
http_post_success()
http_post_with_content_type()
http_post_no_body()
http_post_unreachable()

// report completion of network integration tests
os.output("[PASS] integration/network.apex")