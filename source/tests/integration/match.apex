// source/tests/conditionals/match-case/match_case.apex
// Implementation of Match/Case Constant Branch Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: first number case matches (404)
function match_number_first()
    variable = 404
    result = ""
    match variable
        case 404
            result = "not_found"
        case 402
            result = "payment_required"
        case
            result = "other"
    assert_equal(result, "not_found", "number first (404)")

// test: middle number case matches (402)
function match_number_middle()
    variable = 402
    result = ""
    match variable
        case 404
            result = "not_found"
        case 402
            result = "payment_required"
        case
            result = "other"
    assert_equal(result, "payment_required", "number middle (402)")

// test: default case executes for unmatched number
function match_number_default()
    variable = 500
    result = ""
    match variable
        case 404
            result = "not_found"
        case 402
            result = "payment_required"
        case
            result = "other"
    assert_equal(result, "other", "number default (500)")

// test: negative number case matches
function match_number_negative()
    variable = -1
    result = ""
    match variable
        case -1
            result = "negative_one"
        case 1
            result = "one"
        case
            result = "other"
    assert_equal(result, "negative_one", "number negative (-1)")

// test: zero case matches
function match_number_zero()
    variable = 0
    result = ""
    match variable
        case 0
            result = "zero"
        case 1
            result = "one"
        case
            result = "other"
    assert_equal(result, "zero", "number zero (0)")

// test: first string case matches
function match_string_first()
    variable = "alpha"
    result = ""
    match variable
        case "alpha"
            result = "first"
        case "beta"
            result = "second"
        case
            result = "other"
    assert_equal(result, "first", "string first (alpha)")

// test: second string case matches
function match_string_second()
    variable = "beta"
    result = ""
    match variable
        case "alpha"
            result = "first"
        case "beta"
            result = "second"
        case
            result = "other"
    assert_equal(result, "second", "string second (beta)")

// test: default case executes for unmatched string
function match_string_default()
    variable = "gamma"
    result = ""
    match variable
        case "alpha"
            result = "first"
        case "beta"
            result = "second"
        case
            result = "other"
    assert_equal(result, "other", "string default (gamma)")

// test: empty string case matches
function match_string_empty()
    variable = ""
    result = ""
    match variable
        case ""
            result = "empty"
        case "x"
            result = "x"
        case
            result = "other"
    assert_equal(result, "empty", "string empty")

// test: boolean true case matches
function match_bool_true()
    variable = true
    result = ""
    match variable
        case true
            result = "yes"
        case false
            result = "no"
        case
            result = "other"
    assert_equal(result, "yes", "boolean true")

// test: boolean false case matches
function match_bool_false()
    variable = false
    result = ""
    match variable
        case true
            result = "yes"
        case false
            result = "no"
        case
            result = "other"
    assert_equal(result, "no", "boolean false")

// test: none case matches
function match_none()
    variable = none
    result = ""
    match variable
        case none
            result = "empty"
        case
            result = "other"
    assert_equal(result, "empty", "none literal")

// test: match without default on unmatched value yields empty result
function match_without_default()
    variable = 999
    result = "unchanged"
    match variable
        case 1
            result = "one"
        case 2
            result = "two"
    assert_equal(result, "unchanged", "no default, unmatched")

// test: match without default on matched value updates result
function match_without_default_hit()
    variable = 2
    result = "unchanged"
    match variable
        case 1
            result = "one"
        case 2
            result = "two"
    assert_equal(result, "two", "no default, matched")

// test: large number case matches
function match_large_number()
    variable = 1000000
    result = ""
    match variable
        case 1000000
            result = "million"
        case 999999
            result = "almost"
        case
            result = "other"
    assert_equal(result, "million", "large number (1000000)")

// test: float case matches
function match_float()
    variable = 3.14
    result = ""
    match variable
        case 3.14
            result = "pi"
        case 2.71
            result = "e"
        case
            result = "other"
    assert_equal(result, "pi", "float (3.14)")

// test: negative float case matches
function match_negative_float()
    variable = -2.5
    result = ""
    match variable
        case -2.5
            result = "neg"
        case 2.5
            result = "pos"
        case
            result = "other"
    assert_equal(result, "neg", "negative float (-2.5)")

// test: match result used in expression position
function match_as_expression_style()
    variable = 3
    result = 0
    match variable
        case 1
            result = 10
        case 2
            result = 20
        case 3
            result = 30
        case
            result = -1
    assert_equal(result, 30, "match assigns value (3)")

// test: nested match statements
function match_nested()
    outer = 1
    inner = 2
    result = ""
    match outer
        case 1
            match inner
                case 1
                    result = "1-1"
                case 2
                    result = "1-2"
                case
                    result = "1-other"
        case 2
            result = "2-x"
        case
            result = "other"
    assert_equal(result, "1-2", "nested match (1,2)")

// test: match inside function with early return
function match_with_return()
    variable = 404
    match variable
        case 404
            return "not_found"
        case 402
            return "payment_required"
        case
            return "other"

// test: match with return in all branches (no fallthrough)
function match_return_all()
    match match_with_return()
        case "not_found"
            assert_equal("not_found", "not_found", "return 404")
        case "payment_required"
            assert_equal("payment_required", "payment_required", "return 402")
        case
            assert_equal("other", "not_found", "return default")

// test: match with early break inside loop
function match_with_break()
    found = 0
    for i = 1, 10
        match i
            case 5
                found = i
                break
            case
                found = found
    assert_equal(found, 5, "match with break")

// test: match with continue inside loop
function match_with_continue()
    sum = 0
    for i = 1, 5
        match i
            case 2
                continue
            case 4
                continue
            case
                sum = sum + i
    assert_equal(sum, 9, "match with continue (1+3+5)")

// execute all match/case test cases
match_number_first()
match_number_middle()
match_number_default()
match_number_negative()
match_number_zero()
match_string_first()
match_string_second()
match_string_default()
match_string_empty()
match_bool_true()
match_bool_false()
match_none()
match_without_default()
match_without_default_hit()
match_large_number()
match_float()
match_negative_float()
match_as_expression_style()
match_nested()
match_return_all()
match_with_break()
match_with_continue()

// report completion of match/case tests
os.output("[PASS] integration/match.apex")