// source/tests/loops/for_counter.apex
// Implementation of Counter For Loop Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: basic forward iteration from 1 to 5
function for_basic_forward()
    result = ""
    for i = 1, 5
        result = "{result}{i}"
    assert_equal(result, "12345", "basic forward 1 to 5")

// test: forward iteration with custom step (2)
function for_forward_step()
    result = ""
    for i = 0, 10, 2
        result = "{result}{i}"
    assert_equal(result, "0246810", "forward step 2")

// test: backward iteration from 5 to 1
function for_backward()
    result = ""
    for i = 5, 1, -1
        result = "{result}{i}"
    assert_equal(result, "54321", "backward 5 to 1")

// test: backward iteration with step -3
function for_backward_step()
    result = ""
    for i = 10, 1, -3
        result = "{result}{i}"
    assert_equal(result, "10741", "backward step 3")

// test: single iteration when start equals end
function for_single()
    result = ""
    for i = 3, 3
        result = "{result}{i}"
    assert_equal(result, "3", "single iteration")

// test: break exits loop early
function for_with_break()
    result = ""
    for i = 1, 5
        if i == 3
            break
        result = "{result}{i}"
    assert_equal(result, "12", "break at 3")

// test: continue skips current iteration
function for_with_continue()
    result = ""
    for i = 1, 5
        if i == 3
            continue
        result = "{result}{i}"
    assert_equal(result, "1245", "skip 3")

// test: forward iteration with negative numbers
function for_negative_forward()
    result = ""
    for i = -5, -1
        result = "{result}{i}"
    assert_equal(result, "-5-4-3-2-1", "negative forward")

// test: backward iteration with negative numbers
function for_negative_backward()
    result = ""
    for i = -1, -5, -1
        result = "{result}{i}"
    assert_equal(result, "-1-2-3-4-5", "negative backward")

// test: range crossing zero
function for_zero_range()
    result = ""
    for i = -2, 2
        result = "{result}{i}"
    assert_equal(result, "-2-1012", "zero range -2 to 2")

// test: large step value
function for_large_step()
    result = ""
    for i = 0, 20, 5
        result = "{result}{i}"
    assert_equal(result, "05101520", "large step 5")

// test: decimal step with floating point
function for_decimal_step()
    result = ""
    for i = 0, 2, 0.5
        result = "{result}{i}"
    assert_equal(result, "00.511.52", "decimal step 0.5")

// test: accumulator pattern summing values
function for_accumulator()
    sum = 0
    for i = 1, 5
        sum = sum + i
    assert_equal(sum, 15, "sum 1 to 5")

// test: building string through concatenation
function for_string_build()
    result = ""
    for i = 1, 3
        result = "{result}{i}"
    assert_equal(result, "123", "string build")

// test: nested counter loops
function for_nested()
    result = ""
    for i = 1, 2
        for j = 1, 2
            result = "{result}{i}{j}"
    assert_equal(result, "11122122", "nested loops 1-2, 1-2")

// execute all counter for loop test cases
for_basic_forward()
for_forward_step()
for_backward()
for_backward_step()
for_single()
for_with_break()
for_with_continue()
for_negative_forward()
for_negative_backward()
for_zero_range()
for_large_step()
for_decimal_step()
for_accumulator()
for_string_build()
for_nested()

// report completion of counter for loop tests
os.output("[PASS] loops/for_counter.apex")