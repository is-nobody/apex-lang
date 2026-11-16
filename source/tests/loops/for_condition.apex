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

// test: step by 3 from 0 to 12
function for_step_by_three()
    result = ""
    counter = 0
    for counter <= 12
        result = "{result}{counter}"
        counter = counter + 3
    assert_equal(result, "036912", "step 3, 0 to 12")

// test: step by 5 from 0 to 25
function for_step_by_five()
    result = ""
    counter = 0
    for counter <= 25
        result = "{result}{counter}"
        counter = counter + 5
    assert_equal(result, "0510152025", "step 5, 0 to 25")

// test: less-than only (exclusive)
function for_less_than()
    result = ""
    counter = 1
    for counter < 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1234", "less than 5")

// test: greater-than only (exclusive)
function for_greater_than()
    result = ""
    counter = 5
    for counter > 1
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "5432", "greater than 1")

// test: not-equal condition
function for_not_equal_condition()
    result = ""
    counter = 1
    for counter != 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1234", "not equal 5")

// test: single value range with <=
function for_single_value_range()
    result = ""
    counter = 3
    for counter <= 3
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "3", "single value range")

// test: float condition counting iterations
function for_float_condition()
    count = 0
    counter = 0.5
    for counter <= 3.0
        count = count + 1
        counter = counter + 0.5
    assert_equal(count, 6, "float condition count")

// test: float step counting iterations
function for_float_step()
    count = 0
    counter = 0.0
    for counter <= 1.0
        count = count + 1
        counter = counter + 0.25
    assert_equal(count, 5, "float quarter steps")

// test: and condition with two variables
function for_and_condition()
    result = ""
    counter = 1
    other = 10
    for counter <= 10 and other > 5
        result = "{result}{counter}"
        counter = counter + 1
        other = other - 1
    assert_equal(result, "12345", "and condition limits")

// test: or condition extends loop range
function for_or_condition()
    result = ""
    counter = 1
    for counter < 5 or counter == 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "or condition extends")

// test: and binds tighter than or
function for_and_or_precedence()
    result = ""
    counter = 1
    for counter < 3 or counter > 10 and counter < 15
        result = "{result}{counter}|"
        counter = counter + 10
    assert_equal(result, "1|11|", "and/or precedence")

// test: multiple and conditions
function for_multiple_and_conditions()
    result = ""
    counter = 1
    lower = 0
    upper = 10
    for counter > lower and counter < upper and counter <= 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "multiple and conditions")

// test: and condition with break
function for_and_with_break()
    result = ""
    counter = 1
    limit = 5
    for counter <= 10 and limit > 0
        result = "{result}{counter}"
        if counter == 3
            break
        counter = counter + 1
        limit = limit - 1
    assert_equal(result, "123", "and condition with break")

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

// test: break at first iteration
function for_break_first_iteration()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 1
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "", "break at first iteration")

// test: break at last iteration
function for_break_last_iteration()
    result = ""
    counter = 1
    for counter <= 3
        if counter == 3
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12", "break at last iteration")

// test: break in if-else
function for_break_in_if_else()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 2
            break
        else
            result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1", "break in if else")

// test: break after sum exceeds threshold
function for_break_after_sum_threshold()
    result = ""
    counter = 1
    sum = 0
    for counter <= 100
        sum = sum + counter
        if sum > 10
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1234", "break after sum exceeds 10")

// test: direct break in loop body
function for_break_direct()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}{counter}"
        counter = counter + 1
        if counter == 4
            break
    assert_equal(result, "123", "direct break")

// test: break in nested condition
function for_break_nested_condition()
    result = ""
    counter = 1
    for counter <= 10
        if counter > 2
            if counter == 5
                break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1234", "break in nested condition")

// test: break in either branch
function for_break_in_both_branches()
    result = ""
    counter = 1
    for counter <= 10
        if counter == 3
            break
        else if counter == 4
            break
        else
            result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12", "break in either branch")

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

// test: continue at first iteration
function for_continue_first_iteration()
    result = ""
    counter = 1
    for counter <= 3
        if counter == 1
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "23", "continue at first iteration")

// test: continue at last iteration
function for_continue_last_iteration()
    result = ""
    counter = 1
    for counter <= 3
        if counter == 3
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12", "continue at last iteration")

// test: skip even numbers
function for_continue_skip_even()
    result = ""
    counter = 1
    for counter <= 6
        if counter % 2 == 0
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "135", "skip even numbers")

// test: skip odd numbers
function for_continue_skip_odd()
    result = ""
    counter = 1
    for counter <= 6
        if counter % 2 != 0
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "246", "skip odd numbers")

// test: multiple continue conditions
function for_continue_multiple_conditions()
    result = ""
    counter = 1
    for counter <= 8
        if counter == 2
            counter = counter + 1
            continue
        else if counter == 5
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "134678", "continue with multiple conditions")

// test: continue after increment at start of body
function for_continue_after_increment()
    result = ""
    counter = 0
    for counter <= 4
        counter = counter + 1
        if counter == 3
            continue
        result = "{result}{counter}"
    assert_equal(result, "1245", "continue after start increment")

// test: continue in either branch
function for_continue_in_both_branches()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 2
            counter = counter + 1
            continue
        else if counter == 4
            counter = counter + 1
            continue
        else
            result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "135", "continue in either branch")

// test: accumulator summing values
function for_accumulator()
    sum = 0
    counter = 1
    for counter <= 5
        sum = sum + counter
        counter = counter + 1
    assert_equal(sum, 15, "sum 1 to 5")

// test: factorial
function for_factorial()
    result = 1
    counter = 1
    for counter <= 5
        result = result * counter
        counter = counter + 1
    assert_equal(result, 120, "factorial of 5")

// test: sum of squares
function for_sum_of_squares()
    sum = 0
    counter = 1
    for counter <= 4
        sum = sum + counter * counter
        counter = counter + 1
    assert_equal(sum, 30, "sum of squares 1 to 4")

// test: sum of cubes
function for_sum_of_cubes()
    sum = 0
    counter = 1
    for counter <= 3
        sum = sum + counter * counter * counter
        counter = counter + 1
    assert_equal(sum, 36, "sum of cubes 1 to 3")

// test: fibonacci sequence accumulation
function for_fibonacci()
    a = 0
    b = 1
    counter = 1
    result = ""
    for counter <= 7
        result = "{result}{a}"
        temp = a + b
        a = b
        b = temp
        counter = counter + 1
    assert_equal(result, "0112358", "fibonacci first 7")

// test: product of range
function for_product_of_range()
    product = 1
    counter = 2
    for counter <= 6
        product = product * counter
        counter = counter + 1
    assert_equal(product, 720, "product of 2 to 6")

// test: sum of even numbers
function for_sum_even_numbers()
    sum = 0
    counter = 1
    for counter <= 10
        if counter % 2 == 0
            sum = sum + counter
        counter = counter + 1
    assert_equal(sum, 30, "sum of even numbers 1 to 10")

// test: sum of odd numbers
function for_sum_odd_numbers()
    sum = 0
    counter = 1
    for counter <= 10
        if counter % 2 != 0
            sum = sum + counter
        counter = counter + 1
    assert_equal(sum, 25, "sum of odd numbers 1 to 10")

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

// test: negative to positive range
function for_negative_to_positive()
    result = ""
    counter = -3
    for counter <= 3
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "-3-2-10123", "negative to positive")

// test: positive to negative range
function for_positive_to_negative()
    result = ""
    counter = 3
    for counter >= -3
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "3210-1-2-3", "positive to negative")

// test: large negative step
function for_negative_step_large()
    result = ""
    counter = 10
    for counter >= -10
        result = "{result}{counter},"
        counter = counter - 5
    assert_equal(result, "10,5,0,-5,-10,", "negative step large")

// test: doubling pattern (geometric progression)
function for_doubling()
    result = ""
    counter = 1
    for counter <= 16
        result = "{result}{counter}"
        counter = counter * 2
    assert_equal(result, "124816", "doubling 1 to 16")

// test: halving pattern count
function for_halving()
    count = 0
    counter = 16
    for counter >= 1
        count = count + 1
        counter = counter / 2
    assert_equal(count, 5, "halving 16 to 1 count")

// test: powers of two
function for_powers_of_two()
    result = ""
    counter = 1
    for counter <= 64
        result = "{result}{counter},"
        counter = counter * 2
    assert_equal(result, "1,2,4,8,16,32,64,", "powers of two")

// test: powers of three
function for_powers_of_three()
    result = ""
    counter = 1
    for counter <= 81
        result = "{result}{counter},"
        counter = counter * 3
    assert_equal(result, "1,3,9,27,81,", "powers of three")

// test: squares of numbers
function for_squares()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}{counter * counter},"
        counter = counter + 1
    assert_equal(result, "1,4,9,16,25,", "squares 1 to 5")

// test: cubes of numbers
function for_cubes()
    result = ""
    counter = 1
    for counter <= 4
        result = "{result}{counter * counter * counter},"
        counter = counter + 1
    assert_equal(result, "1,8,27,64,", "cubes 1 to 4")

// test: condition loop with index access
function for_condition_index_access()
    state = ["pos" = 0, "max" = 5]
    result = ""
    for state["pos"] < state["max"]
        result = "{result}{state['pos']}"
        state["pos"] = state["pos"] + 1
    assert_equal(result, "01234", "condition loop with index access")

// test: break inside if within condition loop
function for_condition_break_in_if()
    state = ["pos" = 0]
    result = ""
    for state["pos"] < 10
        if state["pos"] == 5
            break
        result = "{result}{state['pos']}"
        state["pos"] = state["pos"] + 1
    assert_equal(result, "01234", "break inside if within condition loop")

// test: continue inside if within condition loop
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

// test: condition uses two separate tables
function for_table_nested_access()
    outer = ["pos" = 0]
    inner = ["limit" = 4]
    result = ""
    for outer["pos"] < inner["limit"]
        result = "{result}{outer['pos']}"
        outer["pos"] = outer["pos"] + 1
    assert_equal(result, "0123", "table condition from two tables")

// test: table boolean flag controls appending
function for_table_boolean_flag()
    state = ["active" = true, "count" = 0]
    result = ""
    for state["count"] < 5
        if state["count"] == 3
            state["active"] = false
        if state["active"] == true
            result = "{result}{state['count']}"
        state["count"] = state["count"] + 1
    assert_equal(result, "012", "table boolean flag")

// test: populate table values in loop
function for_table_populate_values()
    state = ["pos" = 0, "a" = 0, "b" = 0, "c" = 0]
    for state["pos"] < 3
        if state["pos"] == 0
            state["a"] = 10
        else if state["pos"] == 1
            state["b"] = 20
        else
            state["c"] = 30
        state["pos"] = state["pos"] + 1
    assert_equal(state["a"] + state["b"] + state["c"], 60, "populate table values")

// test: table-based accumulator
function for_table_value_accumulator()
    state = ["pos" = 0, "total" = 0]
    for state["pos"] < 4
        state["total"] = state["total"] + state["pos"] * 10
        state["pos"] = state["pos"] + 1
    assert_equal(state["total"], 60, "table accumulator")

// test: multiple table keys in condition
function for_table_multiple_keys()
    state = ["start" = 1, "end" = 5, "current" = 1, "result" = ""]
    for state["current"] <= state["end"]
        current = state["current"]
        result = state["result"]
        result = "{result}{current}"
        state["result"] = result
        state["current"] = current + 1
    assert_equal(state["result"], "12345", "multiple table keys")

// test: table initialized values accumulator
function for_table_initialized_values()
    state = ["counter" = 10, "limit" = 3, "sum" = 0]
    for state["counter"] >= state["limit"]
        state["sum"] = state["sum"] + state["counter"]
        state["counter"] = state["counter"] - 1
    assert_equal(state["sum"], 52, "table initialized values")

// test: build string from letter table
function for_build_string_letters()
    letters = ["a", "b", "c", "d"]
    result = ""
    index = 1
    for index <= 4
        result = "{result}{letters[index]}"
        index = index + 1
    assert_equal(result, "abcd", "build string from letters")

// test: repeat character pattern
function for_build_string_repeated()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}*"
        counter = counter + 1
    assert_equal(result, "*****", "repeat character")

// test: reverse table of chars
function for_reverse_string()
    chars = ["h", "e", "l", "l", "o"]
    reversed = ""
    index = 5
    for index >= 1
        reversed = "{reversed}{chars[index]}"
        index = index - 1
    assert_equal(reversed, "olleh", "reverse table of chars")

// test: build comma-separated string
function for_string_separator()
    result = ""
    counter = 1
    for counter <= 5
        if counter > 1
            result = "{result},"
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1,2,3,4,5", "string with separators")

// test: build string using table state
function for_build_string_with_table()
    state = ["pos" = 0, "result" = ""]
    for state["pos"] < 4
        pos = state["pos"]
        result = state["result"]
        result = "{result}{pos},"
        state["result"] = result
        state["pos"] = pos + 1
    assert_equal(state["result"], "0,1,2,3,", "build string with table")

// test: basic nested condition loops
function for_nested_basic()
    result = ""
    i = 1
    for i <= 3
        j = 1
        for j <= 2
            result = "{result}{i}{j},"
            j = j + 1
        i = i + 1
    assert_equal(result, "11,12,21,22,31,32,", "nested basic")

// test: break inner loop only
function for_nested_break_inner()
    result = ""
    i = 1
    for i <= 3
        j = 1
        for j <= 3
            if j == 2
                break
            result = "{result}{i}{j},"
            j = j + 1
        i = i + 1
    assert_equal(result, "11,21,31,", "nested break inner")

// test: continue inner loop only
function for_nested_continue_inner()
    result = ""
    i = 1
    for i <= 2
        j = 1
        for j <= 3
            if j == 2
                j = j + 1
                continue
            result = "{result}{i}{j},"
            j = j + 1
        i = i + 1
    assert_equal(result, "11,13,21,23,", "nested continue inner")

// test: nested loop accumulator
function for_nested_accumulator()
    total = 0
    i = 1
    for i <= 3
        j = 1
        for j <= 3
            total = total + i * j
            j = j + 1
        i = i + 1
    assert_equal(total, 36, "nested accumulator")

// test: multiplication table
function for_nested_multiplication_table()
    result = ""
    i = 1
    for i <= 3
        j = 1
        for j <= 3
            product = i * j
            result = "{result}{product},"
            j = j + 1
        i = i + 1
    assert_equal(result, "1,2,3,2,4,6,3,6,9,", "multiplication table")

// test: three levels of nesting
function for_deeply_nested()
    result = ""
    a = 1
    for a <= 2
        b = 1
        for b <= 2
            c = 1
            for c <= 2
                result = "{result}{a}{b}{c},"
                c = c + 1
            b = b + 1
        a = a + 1
    assert_equal(result, "111,112,121,122,211,212,221,222,", "deeply nested")

// test: break outer loop from inner via flag
function for_nested_break_outer_from_inner()
    result = ""
    i = 1
    done = false
    for i <= 3 and done == false
        j = 1
        for j <= 3
            if i == 2 and j == 2
                done = true
                break
            result = "{result}{i}{j},"
            j = j + 1
        i = i + 1
    assert_equal(result, "11,12,13,21,", "break outer from inner via flag")

// test: break both inner and outer
function for_nested_both_break()
    result = ""
    i = 1
    for i <= 5
        j = 1
        for j <= 5
            if j == 2
                break
            result = "{result}{i}{j},"
            j = j + 1
        if i == 3
            break
        i = i + 1
    assert_equal(result, "11,21,31,", "break inner and outer")

// test: even numbers via modulo
function for_modulo_even()
    result = ""
    counter = 1
    for counter <= 8
        if counter % 2 == 0
            result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "2468", "even numbers")

// test: odd numbers via modulo
function for_modulo_odd()
    result = ""
    counter = 1
    for counter <= 8
        if counter % 2 != 0
            result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1357", "odd numbers")

// test: multiples of three
function for_modulo_three()
    result = ""
    counter = 1
    for counter <= 12
        if counter % 3 == 0
            result = "{result}{counter},"
        counter = counter + 1
    assert_equal(result, "3,6,9,12,", "multiples of 3")

// test: multiples of five
function for_modulo_five()
    result = ""
    counter = 1
    for counter <= 20
        if counter % 5 == 0
            result = "{result}{counter},"
        counter = counter + 1
    assert_equal(result, "5,10,15,20,", "multiples of 5")

// test: fizzbuzz style logic
function for_fizzbuzz_style()
    result = ""
    counter = 1
    for counter <= 15
        if counter % 15 == 0
            result = "{result}FB,"
        else if counter % 3 == 0
            result = "{result}F,"
        else if counter % 5 == 0
            result = "{result}B,"
        else
            result = "{result}{counter},"
        counter = counter + 1
    assert_equal(result, "1,2,F,4,B,F,7,8,F,B,11,F,13,14,FB,", "fizzbuzz")

// test: modulo in condition
function for_condition_with_modulo()
    result = ""
    counter = 3
    for counter % 10 != 0
        result = "{result}{counter},"
        counter = counter + 3
    assert_equal(result, "3,6,9,12,15,18,21,24,27,", "modulo condition")

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

// test: start below end with <
function for_start_below_end()
    result = ""
    counter = 10
    for counter < 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "", "start below end")

// test: start above end with >
function for_start_above_end()
    result = ""
    counter = 0
    for counter > 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "", "start above end")

// test: condition variable changes mid-loop
function for_condition_change_mid_loop()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}{counter}"
        if counter == 3
            counter = 10
        else
            counter = counter + 1
    assert_equal(result, "123", "condition change mid loop")

// test: step larger than range
function for_step_larger_than_range()
    result = ""
    counter = 0
    for counter <= 3
        result = "{result}{counter},"
        counter = counter + 10
    assert_equal(result, "0,", "step larger than range")

// test: multi-statement body
function for_multi_statement_body()
    result = ""
    counter = 1
    for counter <= 3
        a = counter * 2
        b = counter * 3
        result = "{result}{a},{b};"
        counter = counter + 1
    assert_equal(result, "2,3;4,6;6,9;", "multi statement body")

// test: condition true exactly once
function for_condition_always_true_once()
    result = ""
    counter = 1
    for counter <= 1
        result = "{result}x"
        counter = counter + 1
    assert_equal(result, "x", "condition true exactly once")

// test: condition never true
function for_condition_never_true()
    result = ""
    counter = 10
    for counter <= 5
        result = "{result}x"
        counter = counter + 1
    assert_equal(result, "", "condition never true")

// test: boolean variable in condition
function for_condition_with_boolean_var()
    active = true
    counter = 1
    result = ""
    for active == true
        result = "{result}{counter}"
        if counter >= 3
            active = false
        counter = counter + 1
    assert_equal(result, "123", "boolean variable in condition")

// helper: sum from 1 to n
function sum_to_n(n)
    total = 0
    counter = 1
    for counter <= n
        total = total + counter
        counter = counter + 1
    return total

// test: condition loop inside function
function for_inside_function()
    assert_equal(sum_to_n(10), 55, "loop inside function")

// helper: find first number divisible by divisor starting from start
function find_first_divisible(start, divisor)
    counter = start
    for counter <= 100
        if counter % divisor == 0
            return counter
        counter = counter + 1
    return -1

// test: return from inside loop
function for_return_inside_loop()
    assert_equal(find_first_divisible(5, 3), 6, "return from loop")

// helper: double a value
function double_value(x)
    return x * 2

// test: function call in loop body
function for_function_call_in_body()
    result = ""
    counter = 1
    for counter <= 3
        doubled = double_value(counter)
        result = "{result}{doubled},"
        counter = counter + 1
    assert_equal(result, "2,4,6,", "function call in body")

// helper: square a value
function square(x)
    return x * x

// test: accumulate function results
function for_accumulate_function_result()
    total = 0
    counter = 1
    for counter <= 4
        total = total + square(counter)
        counter = counter + 1
    assert_equal(total, 30, "accumulate function result")

// helper: count to n
function count_to_n(n)
    count = 0
    counter = 1
    for counter <= n
        count = count + 1
        counter = counter + 1
    return count

// test: function with loop parameter
function for_function_with_loop_parameter()
    assert_equal(count_to_n(7), 7, "function with loop parameter")

// helper: check if even
function is_even(x)
    return x % 2 == 0

// test: loop calls function
function for_loop_calls_function()
    result = ""
    counter = 1
    for counter <= 6
        if is_even(counter) == true
            result = "{result}{counter},"
        counter = counter + 1
    assert_equal(result, "2,4,6,", "loop calls function")

// test: if-else inside loop
function for_with_if_else()
    result = ""
    counter = 1
    for counter <= 5
        if counter % 2 == 0
            result = "{result}E"
        else
            result = "{result}O"
        counter = counter + 1
    assert_equal(result, "OEOEO", "if else in loop")

// test: else-if chain inside loop
function for_with_else_if()
    result = ""
    counter = 1
    for counter <= 6
        if counter <= 2
            result = "{result}L"
        else if counter <= 4
            result = "{result}M"
        else
            result = "{result}H"
        counter = counter + 1
    assert_equal(result, "LLMMHH", "else if in loop")

// test: ternary inside loop
function for_with_ternary()
    result = ""
    counter = 1
    for counter <= 4
        label = "even" if counter % 2 == 0 else "odd"
        result = "{result}{label},"
        counter = counter + 1
    assert_equal(result, "odd,even,odd,even,", "ternary in loop")

// test: match statement inside loop
function for_with_match()
    result = ""
    counter = 1
    for counter <= 5
        match counter
            case 1
                result = "{result}one,"
            case 2
                result = "{result}two,"
            case 3
                result = "{result}three,"
            case
                result = "{result}many,"
        counter = counter + 1
    assert_equal(result, "one,two,three,many,many,", "match in loop")

// test: nested if inside loop
function for_with_nested_if()
    result = ""
    counter = 1
    for counter <= 6
        if counter % 2 == 0
            if counter <= 3
                result = "{result}small_even,"
            else
                result = "{result}large_even,"
        else
            if counter <= 3
                result = "{result}small_odd,"
            else
                result = "{result}large_odd,"
        counter = counter + 1
    assert_equal(result, "small_odd,small_even,small_odd,large_even,large_odd,large_even,", "nested if in loop")

// test: multiple branches
function for_with_multiple_branches()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 1
            result = "{result}A"
        else if counter == 2
            result = "{result}B"
        else if counter == 3
            result = "{result}C"
        else
            result = "{result}D"
        counter = counter + 1
    assert_equal(result, "ABCDD", "multiple branches")

// test: two counters converging
function for_two_counters()
    result = ""
    up = 1
    down = 10
    for up <= down
        result = "{result}{up}-{down},"
        up = up + 1
        down = down - 1
    assert_equal(result, "1-10,2-9,3-8,4-7,5-6,", "two counters")

// test: counter and accumulator together
function for_counter_and_accumulator()
    counter = 1
    total = 0
    count = 0
    for counter <= 5
        total = total + counter
        count = count + 1
        counter = counter + 1
    assert_equal(total, 15, "counter and accumulator total")
    assert_equal(count, 5, "counter and accumulator count")

// test: counter and string variable
function for_counter_and_string()
    result = ""
    counter = 1
    separator = "-"
    for counter <= 4
        if counter > 1
            result = "{result}{separator}"
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1-2-3-4", "counter and string")

// test: multiple conditions tracked
function for_multiple_conditions_tracking()
    result = ""
    a = 1
    b = 10
    count = 0
    for a < b
        count = count + 1
        result = "{result}{a},"
        a = a + 3
        b = b - 2
    assert_equal(count, 2, "multiple condition tracking count")
    assert_equal(result, "1,4,", "multiple condition tracking result")

// test: interdependent variables (fibonacci-like)
function for_interdependent_variables()
    result = ""
    a = 0
    b = 1
    for a < 20
        result = "{result}{a},"
        a = a + b
        b = a - b
    assert_equal(result, "0,1,1,2,3,5,8,13,", "interdependent variables")

// test: triangle pattern
function for_triangle_pattern()
    result = ""
    row = 1
    for row <= 4
        col = 1
        for col <= row
            result = "{result}*"
            col = col + 1
        result = "{result}\n"
        row = row + 1
    assert_equal(result, "*\n**\n***\n****\n", "triangle pattern")

// test: reverse triangle
function for_reverse_triangle()
    result = ""
    row = 4
    for row >= 1
        col = 1
        for col <= row
            result = "{result}*"
            col = col + 1
        result = "{result}\n"
        row = row - 1
    assert_equal(result, "****\n***\n**\n*\n", "reverse triangle")

// test: pyramid numbers
function for_pyramid_numbers()
    result = ""
    row = 1
    for row <= 3
        col = 1
        for col <= row
            result = "{result}{col}"
            col = col + 1
        result = "{result}\n"
        row = row + 1
    assert_equal(result, "1\n12\n123\n", "pyramid numbers")

// test: alternating sign
function for_alternating_sign()
    result = ""
    counter = 1
    sign = 1
    for counter <= 6
        value = counter * sign
        result = "{result}{value},"
        sign = sign * -1
        counter = counter + 1
    assert_equal(result, "1,-2,3,-4,5,-6,", "alternating sign")

// test: zigzag pattern
function for_zigzag_pattern()
    result = ""
    counter = 1
    direction = 1
    value = 1
    for counter <= 6
        result = "{result}{value},"
        if value == 3
            direction = -1
        else if value == 1 and direction == -1
            direction = 1
        value = value + direction
        counter = counter + 1
    assert_equal(result, "1,2,3,2,1,2,", "zigzag pattern")

// test: arithmetic in condition
function for_complex_arithmetic_condition()
    result = ""
    counter = 1
    for counter * 2 <= 10
        result = "{result}{counter},"
        counter = counter + 1
    assert_equal(result, "1,2,3,4,5,", "arithmetic in condition")

// test: long condition with multiple parts
function for_long_condition()
    result = ""
    counter = 1
    for counter >= 1 and counter <= 100 and counter <= 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "long condition")

// test: condition using multiple table accesses
function for_condition_multiple_table_access()
    state = ["a" = 1, "b" = 5, "limit" = 3]
    result = ""
    for state["a"] < state["b"] and state["a"] <= state["limit"]
        result = "{result}{state['a']}"
        state["a"] = state["a"] + 1
    assert_equal(result, "123", "multiple table access in condition")

// execute all condition for loop test cases
for_basic_count_up()
for_count_down()
for_with_step()
for_step_by_three()
for_step_by_five()
for_less_than()
for_greater_than()
for_not_equal_condition()
for_single_value_range()
for_float_condition()
for_float_step()
for_and_condition()
for_or_condition()
for_and_or_precedence()
for_multiple_and_conditions()
for_and_with_break()
for_with_break()
for_break_first_iteration()
for_break_last_iteration()
for_break_in_if_else()
for_break_after_sum_threshold()
for_break_direct()
for_break_nested_condition()
for_break_in_both_branches()
for_with_continue()
for_continue_first_iteration()
for_continue_last_iteration()
for_continue_skip_even()
for_continue_skip_odd()
for_continue_multiple_conditions()
for_continue_after_increment()
for_continue_in_both_branches()
for_accumulator()
for_factorial()
for_sum_of_squares()
for_sum_of_cubes()
for_fibonacci()
for_product_of_range()
for_sum_even_numbers()
for_sum_odd_numbers()
for_negative()
for_negative_decrement()
for_negative_to_positive()
for_positive_to_negative()
for_negative_step_large()
for_doubling()
for_halving()
for_powers_of_two()
for_powers_of_three()
for_squares()
for_cubes()
for_condition_index_access()
for_condition_break_in_if()
for_condition_continue_in_if()
for_condition_nested_if_break()
for_table_nested_access()
for_table_boolean_flag()
for_table_populate_values()
for_table_value_accumulator()
for_table_multiple_keys()
for_table_initialized_values()
for_build_string_letters()
for_build_string_repeated()
for_reverse_string()
for_string_separator()
for_build_string_with_table()
for_nested_basic()
for_nested_break_inner()
for_nested_continue_inner()
for_nested_accumulator()
for_nested_multiplication_table()
for_deeply_nested()
for_nested_break_outer_from_inner()
for_nested_both_break()
for_modulo_even()
for_modulo_odd()
for_modulo_three()
for_modulo_five()
for_fizzbuzz_style()
for_condition_with_modulo()
for_single()
for_zero()
for_start_below_end()
for_start_above_end()
for_condition_change_mid_loop()
for_step_larger_than_range()
for_multi_statement_body()
for_condition_always_true_once()
for_condition_never_true()
for_condition_with_boolean_var()
for_inside_function()
for_return_inside_loop()
for_function_call_in_body()
for_accumulate_function_result()
for_function_with_loop_parameter()
for_loop_calls_function()
for_with_if_else()
for_with_else_if()
for_with_ternary()
for_with_match()
for_with_nested_if()
for_with_multiple_branches()
for_two_counters()
for_counter_and_accumulator()
for_counter_and_string()
for_multiple_conditions_tracking()
for_interdependent_variables()
for_triangle_pattern()
for_reverse_triangle()
for_pyramid_numbers()
for_alternating_sign()
for_zigzag_pattern()
for_complex_arithmetic_condition()
for_long_condition()
for_condition_multiple_table_access()

// report completion of conditional for loop tests
os.output("[PASS] loops/for_condition.apex")