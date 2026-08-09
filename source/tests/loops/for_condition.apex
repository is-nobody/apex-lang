// source/tests/loops/for_condition.apex
// Implementation of Conditional For Loop Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: basic increment from 1 to 5
function for_basic_count_up()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "basic count up 1 to 5")

// test: decrement from 5 down to 1
function for_count_down()
    result = ""
    counter = 5
    for counter >= 1
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "54321", "count down 5 to 1")

// test: step by 2 from 0 to 10
function for_with_step()
    result = ""
    counter = 0
    for counter <= 10
        result = "{result}{counter}"
        counter = counter + 2
    assert_equal(result, "0246810", "step 2, 0 to 10")

// test: break exits loop early
function for_with_break()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 3
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12", "break at 3")

// test: continue skips current iteration
function for_with_continue()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 3
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1245", "skip 3")

// test: single iteration loop
function for_single()
    result = ""
    counter = 1
    for counter <= 1
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1", "single iteration")

// test: condition false from start, zero iterations
function for_zero()
    result = ""
    counter = 1
    for counter > 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "", "zero iterations")

// test: accumulator pattern summing values
function for_accumulator()
    sum = 0
    counter = 1
    for counter <= 5
        sum = sum + counter
        counter = counter + 1
    assert_equal(sum, 15, "sum 1 to 5")

// test: iteration with negative numbers (increment)
function for_negative()
    result = ""
    counter = -5
    for counter <= -1
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "-5-4-3-2-1", "negative count up")

// test: iteration with negative numbers (decrement)
function for_negative_decrement()
    result = ""
    counter = -1
    for counter >= -5
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "-1-2-3-4-5", "negative count down")

// test: doubling pattern (geometric progression)
function for_doubling()
    result = ""
    counter = 1
    for counter <= 16
        result = "{result}{counter}"
        counter = counter * 2
    assert_equal(result, "124816", "doubling 1 to 16")

// test: table index as loop condition variable
function for_condition_index_access()
    state = ["pos" = 0, "max" = 5]
    result = ""
    for state["pos"] < state["max"]
        result = "{result}{state['pos']}"
        state["pos"] = state["pos"] + 1
    assert_equal(result, "01234", "condition loop with index access")

// test: break inside nested if within conditional loop
function for_condition_break_in_if()
    state = ["pos" = 0]
    result = ""
    for state["pos"] < 10
        if state["pos"] == 5
            break
        result = "{result}{state['pos']}"
        state["pos"] = state["pos"] + 1
    assert_equal(result, "01234", "break inside if within condition loop")

// test: continue inside nested if within conditional loop
function for_condition_continue_in_if()
    state = ["pos" = 0]
    result = ""
    for state["pos"] < 6
        state["pos"] = state["pos"] + 1
        if state["pos"] == 3
            continue
        result = "{result}{state['pos']}"
    assert_equal(result, "12456", "continue inside if within condition loop")

// test: nested if with break in deeper level
function for_condition_nested_if_break()
    state = ["pos" = 0, "limit" = 8]
    result = ""
    for state["pos"] < state["limit"]
        if state["pos"] > 2
            if state["pos"] == 5
                break
        result = "{result}{state['pos']}"
        state["pos"] = state["pos"] + 1
    assert_equal(result, "01234", "nested if with break in condition loop")

// execute all conditional for loop test cases
for_basic_count_up()
for_count_down()
for_with_step()
for_with_break()
for_with_continue()
for_single()
for_zero()
for_accumulator()
for_negative()
for_negative_decrement()
for_doubling()
for_condition_index_access()
for_condition_break_in_if()
for_condition_continue_in_if()
for_condition_nested_if_break()

// report completion of conditional for loop tests
os.output("[PASS] loops/for_condition.apex")