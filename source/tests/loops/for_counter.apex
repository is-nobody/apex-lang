// source/tests/loops/for_counter.apex
// Implementation of Counter For Loop Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: forward 1 to 5
function counter_forward_1_5()
    result = ""
    for i = 1, 5
        result = "{result}{i}"
    assert_equal(result, "12345", "forward 1 to 5")

// test: forward 1 to 1 (single)
function counter_forward_1_1()
    result = ""
    for i = 1, 1
        result = "{result}{i}"
    assert_equal(result, "1", "forward 1 to 1")

// test: forward 0 to 0 (single at zero)
function counter_forward_0_0()
    result = ""
    for i = 0, 0
        result = "{result}{i}"
    assert_equal(result, "0", "forward 0 to 0")

// test: forward 1 to 2 (two iterations)
function counter_forward_1_2()
    result = ""
    for i = 1, 2
        result = "{result}{i}"
    assert_equal(result, "12", "forward 1 to 2")

// test: forward 1 to 3
function counter_forward_1_3()
    result = ""
    for i = 1, 3
        result = "{result}{i}"
    assert_equal(result, "123", "forward 1 to 3")

// test: forward 1 to 10
function counter_forward_1_10()
    result = ""
    for i = 1, 10
        result = "{result}{i}"
    assert_equal(result, "12345678910", "forward 1 to 10")

// test: forward 0 to 4 (starts at zero)
function counter_forward_0_4()
    result = ""
    for i = 0, 4
        result = "{result}{i}"
    assert_equal(result, "01234", "forward 0 to 4")

// test: forward 3 to 7 (mid-range start)
function counter_forward_3_7()
    result = ""
    for i = 3, 7
        result = "{result}{i}"
    assert_equal(result, "34567", "forward 3 to 7")

// test: forward 100 to 103 (large start)
function counter_forward_100_103()
    result = ""
    for i = 100, 103
        result = "{result}{i},"
    assert_equal(result, "100,101,102,103,", "forward 100 to 103")

// test: forward 7 to 7 (start equals end)
function counter_forward_same()
    result = ""
    for i = 7, 7
        result = "{result}{i}"
    assert_equal(result, "7", "forward same start/end")

// test: backward 5 to 1
function counter_backward_5_1()
    result = ""
    for i = 5, 1, -1
        result = "{result}{i}"
    assert_equal(result, "54321", "backward 5 to 1")

// test: backward 3 to 3 (single)
function counter_backward_single()
    result = ""
    for i = 3, 3, -1
        result = "{result}{i}"
    assert_equal(result, "3", "backward single 3 to 3")

// test: backward 10 to 1
function counter_backward_10_1()
    result = ""
    for i = 10, 1, -1
        result = "{result}{i},"
    assert_equal(result, "10,9,8,7,6,5,4,3,2,1,", "backward 10 to 1")

// test: backward 5 to 2
function counter_backward_5_2()
    result = ""
    for i = 5, 2, -1
        result = "{result}{i}"
    assert_equal(result, "5432", "backward 5 to 2")

// test: backward 100 to 97
function counter_backward_100_97()
    result = ""
    for i = 100, 97, -1
        result = "{result}{i},"
    assert_equal(result, "100,99,98,97,", "backward 100 to 97")

// test: backward 0 to -3
function counter_backward_zero()
    result = ""
    for i = 0, -3, -1
        result = "{result}{i},"
    assert_equal(result, "0,-1,-2,-3,", "backward 0 to -3")

// test: backward 2 to 0
function counter_backward_2_0()
    result = ""
    for i = 2, 0, -1
        result = "{result}{i}"
    assert_equal(result, "210", "backward 2 to 0")

// test: backward -1 to -3
function counter_backward_neg()
    result = ""
    for i = -1, -3, -1
        result = "{result}{i},"
    assert_equal(result, "-1,-2,-3,", "backward -1 to -3")

// test: step 2 from 0 to 10
function counter_step_2()
    result = ""
    for i = 0, 10, 2
        result = "{result}{i}"
    assert_equal(result, "0246810", "step 2 from 0 to 10")

// test: step 2 from 1 to 9
function counter_step_2_odd()
    result = ""
    for i = 1, 9, 2
        result = "{result}{i}"
    assert_equal(result, "13579", "step 2 odd 1 to 9")

// test: step 3 from 0 to 12
function counter_step_3()
    result = ""
    for i = 0, 12, 3
        result = "{result}{i},"
    assert_equal(result, "0,3,6,9,12,", "step 3 from 0 to 12")

// test: step 5 from 0 to 20
function counter_step_5()
    result = ""
    for i = 0, 20, 5
        result = "{result}{i},"
    assert_equal(result, "0,5,10,15,20,", "step 5 from 0 to 20")

// test: step 10 from 0 to 50
function counter_step_10()
    result = ""
    for i = 0, 50, 10
        result = "{result}{i},"
    assert_equal(result, "0,10,20,30,40,50,", "step 10 from 0 to 50")

// test: step larger than range
function counter_step_overshoot()
    result = ""
    for i = 1, 5, 100
        result = "{result}{i},"
    assert_equal(result, "1,", "step 100 over range 1 to 5")

// test: step exactly range length
function counter_step_exact()
    result = ""
    for i = 1, 5, 4
        result = "{result}{i}"
    assert_equal(result, "15", "step 4 over range 1 to 5")

// test: step exactly range length minus one
function counter_step_almost()
    result = ""
    for i = 1, 5, 3
        result = "{result}{i}"
    assert_equal(result, "14", "step 3 over range 1 to 5")

// test: step 2 with non-zero start
function counter_step_2_start5()
    result = ""
    for i = 5, 15, 2
        result = "{result}{i},"
    assert_equal(result, "5,7,9,11,13,15,", "step 2 from 5 to 15")

// test: step -2 from 10 to 0
function counter_step_neg_2()
    result = ""
    for i = 10, 0, -2
        result = "{result}{i}"
    assert_equal(result, "1086420", "backward step 2 from 10 to 0")

// test: step -2 from 9 to 1
function counter_step_neg_2_odd()
    result = ""
    for i = 9, 1, -2
        result = "{result}{i}"
    assert_equal(result, "97531", "backward step 2 odd 9 to 1")

// test: step -3 from 10 to 1
function counter_step_neg_3()
    result = ""
    for i = 10, 1, -3
        result = "{result}{i},"
    assert_equal(result, "10,7,4,1,", "backward step 3 from 10 to 1")

// test: step -5 from 20 to 0
function counter_step_neg_5()
    result = ""
    for i = 20, 0, -5
        result = "{result}{i},"
    assert_equal(result, "20,15,10,5,0,", "backward step 5 from 20 to 0")

// test: backward step larger than range
function counter_step_neg_overshoot()
    result = ""
    for i = 5, 1, -100
        result = "{result}{i},"
    assert_equal(result, "5,", "backward step 100 over 5 to 1")

// test: backward step exact
function counter_step_neg_exact()
    result = ""
    for i = 5, 1, -4
        result = "{result}{i}"
    assert_equal(result, "51", "backward step 4 over 5 to 1")

// test: negative forward from -5 to -1
function counter_neg_forward()
    result = ""
    for i = -5, -1
        result = "{result}{i},"
    assert_equal(result, "-5,-4,-3,-2,-1,", "negative forward -5 to -1")

// test: negative backward from -1 to -5
function counter_neg_backward()
    result = ""
    for i = -1, -5, -1
        result = "{result}{i},"
    assert_equal(result, "-1,-2,-3,-4,-5,", "negative backward -1 to -5")

// test: cross zero forward -3 to 3
function counter_cross_zero()
    result = ""
    for i = -3, 3
        result = "{result}{i},"
    assert_equal(result, "-3,-2,-1,0,1,2,3,", "cross zero forward")

// test: cross zero backward 3 to -3
function counter_cross_zero_back()
    result = ""
    for i = 3, -3, -1
        result = "{result}{i},"
    assert_equal(result, "3,2,1,0,-1,-2,-3,", "cross zero backward")

// test: negative to positive step 2
function counter_neg_pos_step()
    result = ""
    for i = -6, 6, 3
        result = "{result}{i},"
    assert_equal(result, "-6,-3,0,3,6,", "negative to positive step 3")

// test: positive to negative step -3
function counter_pos_neg_step()
    result = ""
    for i = 6, -6, -3
        result = "{result}{i},"
    assert_equal(result, "6,3,0,-3,-6,", "positive to negative step 3")

// test: all negative with step
function counter_neg_with_step()
    result = ""
    for i = -10, -2, 2
        result = "{result}{i},"
    assert_equal(result, "-10,-8,-6,-4,-2,", "negative with step 2")

// test: very negative range
function counter_neg_large()
    result = ""
    for i = -100, -96
        result = "{result}{i},"
    assert_equal(result, "-100,-99,-98,-97,-96,", "very negative range")

// test: step 0.5 from 0 to 2
function counter_step_half()
    result = ""
    for i = 0, 2, 0.5
        result = "{result}{i},"
    assert_equal(result, "0,0.5,1,1.5,2,", "step 0.5 from 0 to 2")

// test: step 0.5 from 1 to 3
function counter_step_half_1_3()
    result = ""
    for i = 1, 3, 0.5
        result = "{result}{i},"
    assert_equal(result, "1,1.5,2,2.5,3,", "step 0.5 from 1 to 3")

// test: step 0.5 from 0 to 1
function counter_step_half_0_1()
    result = ""
    for i = 0, 1, 0.5
        result = "{result}{i},"
    assert_equal(result, "0,0.5,1,", "step 0.5 from 0 to 1")

// test: backward step -0.5
function counter_step_neg_half()
    result = ""
    for i = 2, 0, -0.5
        result = "{result}{i},"
    assert_equal(result, "2,1.5,1,0.5,0,", "backward step 0.5")

// test: forward with start > end (zero iterations)
function counter_empty_forward()
    result = ""
    for i = 5, 1
        result = "{result}{i}"
    assert_equal(result, "", "empty forward 5 to 1")

// test: backward with start < end (zero iterations)
function counter_empty_backward()
    result = ""
    for i = 1, 5, -1
        result = "{result}{i}"
    assert_equal(result, "", "empty backward 1 to 5")

// test: forward positive step with start > end
function counter_empty_pos_step()
    result = ""
    for i = 10, 1, 2
        result = "{result}{i}"
    assert_equal(result, "", "empty positive step 10 to 1")

// test: backward negative step with start < end
function counter_empty_neg_step()
    result = ""
    for i = 1, 10, -2
        result = "{result}{i}"
    assert_equal(result, "", "empty negative step 1 to 10")

// test: zero range with step 2
function counter_zero_range_step()
    result = ""
    for i = 0, 0, 2
        result = "{result}{i},"
    assert_equal(result, "0,", "zero range step 2")

// test: single iteration with larger step
function counter_single_step()
    result = ""
    for i = 5, 5, 10
        result = "{result}{i},"
    assert_equal(result, "5,", "single iteration larger step")

// test: large start and end
function counter_large_range()
    result = ""
    for i = 1000000, 1000003
        result = "{result}{i},"
    assert_equal(result, "1000000,1000001,1000002,1000003,", "large range")

// test: loop with many iterations (bounded)
function counter_many_iterations()
    count = 0
    for i = 1, 100
        count = count + 1
    assert_equal(count, 100, "100 iterations")

// test: break at first iteration
function counter_break_first()
    result = ""
    for i = 1, 5
        if i == 1
            break
        result = "{result}{i}"
    assert_equal(result, "", "break at first iteration")

// test: break at middle (3)
function counter_break_middle()
    result = ""
    for i = 1, 5
        if i == 3
            break
        result = "{result}{i}"
    assert_equal(result, "12", "break at 3")

// test: break at last iteration
function counter_break_last()
    result = ""
    for i = 1, 5
        if i == 5
            break
        result = "{result}{i}"
    assert_equal(result, "1234", "break at 5")

// test: break just before last
function counter_break_before_last()
    result = ""
    for i = 1, 5
        if i == 4
            break
        result = "{result}{i}"
    assert_equal(result, "123", "break at 4")

// test: break with negative step
function counter_break_neg()
    result = ""
    for i = 5, 1, -1
        if i == 3
            break
        result = "{result}{i}"
    assert_equal(result, "54", "break at 3 in backward")

// test: break with step 2
function counter_break_step2()
    result = ""
    for i = 0, 10, 2
        if i == 6
            break
        result = "{result}{i},"
    assert_equal(result, "0,2,4,", "break at 6 with step 2")

// test: break in if-else
function counter_break_if_else()
    result = ""
    for i = 1, 5
        if i == 2
            break
        else
            result = "{result}{i}"
    assert_equal(result, "1", "break in if-else at 2")

// test: break after accumulator threshold
function counter_break_threshold()
    sum = 0
    for i = 1, 100
        sum = sum + i
        if sum > 10
            break
    assert_equal(sum, 15, "sum exceeds 10 at i=5")

// test: break from deep nesting
function counter_break_deep()
    result = ""
    for i = 1, 10
        if i > 2
            if i == 5
                break
        result = "{result}{i}"
    assert_equal(result, "1234", "break at 5 from deep nesting")

// test: break in match
function counter_break_in_match()
    result = ""
    for i = 1, 5
        match i
            case 3
                break
            case
                result = "{result}{i}"
    assert_equal(result, "12", "break in match at 3")

// test: continue at first
function counter_continue_first()
    result = ""
    for i = 1, 5
        if i == 1
            continue
        result = "{result}{i}"
    assert_equal(result, "2345", "skip 1")

// test: continue at middle
function counter_continue_middle()
    result = ""
    for i = 1, 5
        if i == 3
            continue
        result = "{result}{i}"
    assert_equal(result, "1245", "skip 3")

// test: continue at last
function counter_continue_last()
    result = ""
    for i = 1, 5
        if i == 5
            continue
        result = "{result}{i}"
    assert_equal(result, "1234", "skip 5")

// test: skip even numbers
function counter_continue_even()
    result = ""
    for i = 1, 10
        if i % 2 == 0
            continue
        result = "{result}{i},"
    assert_equal(result, "1,3,5,7,9,", "skip even")

// test: skip odd numbers
function counter_continue_odd()
    result = ""
    for i = 1, 10
        if i % 2 != 0
            continue
        result = "{result}{i},"
    assert_equal(result, "2,4,6,8,10,", "skip odd")

// test: skip multiples of 3
function counter_continue_mult3()
    result = ""
    for i = 1, 12
        if i % 3 == 0
            continue
        result = "{result}{i},"
    assert_equal(result, "1,2,4,5,7,8,10,11,", "skip multiples of 3")

// test: skip multiples of 5
function counter_continue_mult5()
    result = ""
    for i = 1, 20
        if i % 5 == 0
            continue
        result = "{result}{i},"
    assert_equal(result, "1,2,3,4,6,7,8,9,11,12,13,14,16,17,18,19,", "skip multiples of 5")

// test: continue with step 2
function counter_continue_step2()
    result = ""
    for i = 0, 10, 2
        if i == 4
            continue
        result = "{result}{i},"
    assert_equal(result, "0,2,6,8,10,", "skip 4 with step 2")

// test: continue after accumulator
function counter_continue_accum()
    sum = 0
    for i = 1, 10
        sum = sum + i
        if i == 5
            continue
        sum = sum + 100
    assert_equal(sum, 955, "continue changes sum")

// test: continue in match
function counter_continue_in_match()
    result = ""
    for i = 1, 5
        match i
            case 3
                continue
            case
                result = "{result}{i}"
    assert_equal(result, "1245", "continue in match at 3")

// test: sum 1 to 5
function counter_sum_1_5()
    sum = 0
    for i = 1, 5
        sum = sum + i
    assert_equal(sum, 15, "sum 1 to 5")

// test: sum 1 to 10
function counter_sum_1_10()
    sum = 0
    for i = 1, 10
        sum = sum + i
    assert_equal(sum, 55, "sum 1 to 10")

// test: sum 1 to 100
function counter_sum_1_100()
    sum = 0
    for i = 1, 100
        sum = sum + i
    assert_equal(sum, 5050, "sum 1 to 100")

// test: sum 0 to 9
function counter_sum_0_9()
    sum = 0
    for i = 0, 9
        sum = sum + i
    assert_equal(sum, 45, "sum 0 to 9")

// test: sum of even numbers 1 to 10
function counter_sum_even()
    sum = 0
    for i = 1, 10
        if i % 2 == 0
            sum = sum + i
    assert_equal(sum, 30, "sum even 1 to 10")

// test: sum of odd numbers 1 to 10
function counter_sum_odd()
    sum = 0
    for i = 1, 10
        if i % 2 != 0
            sum = sum + i
    assert_equal(sum, 25, "sum odd 1 to 10")

// test: sum of squares 1 to 5
function counter_sum_squares()
    sum = 0
    for i = 1, 5
        sum = sum + i * i
    assert_equal(sum, 55, "sum squares 1 to 5")

// test: sum of cubes 1 to 3
function counter_sum_cubes()
    sum = 0
    for i = 1, 3
        sum = sum + i * i * i
    assert_equal(sum, 36, "sum cubes 1 to 3")

// test: count evens
function counter_count_evens()
    count = 0
    for i = 1, 20
        if i % 2 == 0
            count = count + 1
    assert_equal(count, 10, "count evens 1 to 20")

// test: count odds
function counter_count_odds()
    count = 0
    for i = 1, 20
        if i % 2 != 0
            count = count + 1
    assert_equal(count, 10, "count odds 1 to 20")

// test: double sum with accumulator
function counter_double_sum()
    total = 0
    for i = 1, 4
        total = total + i * 10
    assert_equal(total, 100, "sum 10,20,30,40")

// test: alternating sum
function counter_alternating_sum()
    sum = 0
    sign = 1
    for i = 1, 5
        sum = sum + i * sign
        sign = sign * -1
    assert_equal(sum, 3, "1-2+3-4+5")

// test: geometric sum (powers of 2)
function counter_geom_sum()
    sum = 0
    for i = 1, 4
        sum = sum + i * 2
    assert_equal(sum, 20, "sum 2,4,6,8")

// test: count positive values
function counter_count_positive()
    values = [5, -3, 8, 0, -2, 7]
    count = 0
    for i = 1, 6
        if values[i] > 0
            count = count + 1
    assert_equal(count, 3, "count positives in table")

// test: accumulate maximum
function counter_find_max()
    values = [3, 7, 2, 9, 5]
    largest = 0
    for i = 1, 5
        if values[i] > largest
            largest = values[i]
    assert_equal(largest, 9, "find max in table")

// test: build string of numbers 1 to 3
function counter_build_123()
    result = ""
    for i = 1, 3
        result = "{result}{i}"
    assert_equal(result, "123", "build 123")

// test: build with separators
function counter_build_sep()
    result = ""
    for i = 1, 5
        if i > 1
            result = "{result},"
        result = "{result}{i}"
    assert_equal(result, "1,2,3,4,5", "build with separators")

// test: build with trailing separator
function counter_build_trailing()
    result = ""
    for i = 1, 4
        result = "{result}{i},"
    assert_equal(result, "1,2,3,4,", "build with trailing sep")

// test: build with prefix
function counter_build_prefix()
    result = ""
    for i = 1, 3
        result = "{result}item{i} "
    assert_equal(result, "item1 item2 item3 ", "build with prefix")

// test: build repeated character
function counter_build_star()
    result = ""
    for i = 1, 5
        result = "{result}*"
    assert_equal(result, "*****", "build 5 stars")

// test: build characters from a table
function counter_build_chars()
    chars = ["a", "b", "c", "d", "e"]
    result = ""
    for i = 1, 5
        result = "{result}{chars[i]}"
    assert_equal(result, "abcde", "build from chars table")

// test: build with table lookup
function counter_build_lookup()
    words = ["one", "two", "three", "four", "five"]
    result = ""
    for i = 1, 5
        result = "{result}{words[i]},"
    assert_equal(result, "one,two,three,four,five,", "build with lookup")

// test: build with condition
function counter_build_conditional()
    result = ""
    for i = 1, 6
        if i % 2 == 0
            result = "{result}E"
        else
            result = "{result}O"
    assert_equal(result, "OEOEOE", "build with even/odd")

// test: reverse build
function counter_build_reverse()
    result = ""
    for i = 5, 1, -1
        result = "{result}{i}"
    assert_equal(result, "54321", "build reverse")

// test: build CSV-ish
function counter_build_csv()
    result = ""
    for i = 1, 3
        result = "{result}v{i}"
        if i < 3
            result = "{result},"
    assert_equal(result, "v1,v2,v3", "build csv-like")

// test: nested 2x2
function counter_nested_2_2()
    result = ""
    for i = 1, 2
        for j = 1, 2
            result = "{result}{i}{j}"
    assert_equal(result, "11122122", "nested 2x2")

// test: nested 3x2
function counter_nested_3_2()
    result = ""
    for i = 1, 3
        for j = 1, 2
            result = "{result}{i}{j},"
    assert_equal(result, "11,12,21,22,31,32,", "nested 3x2")

// test: nested 2x3
function counter_nested_2_3()
    result = ""
    for i = 1, 2
        for j = 1, 3
            result = "{result}{i}{j},"
    assert_equal(result, "11,12,13,21,22,23,", "nested 2x3")

// test: nested 3x3
function counter_nested_3_3()
    result = ""
    for i = 1, 3
        for j = 1, 3
            result = "{result}{i}{j},"
    assert_equal(result, "11,12,13,21,22,23,31,32,33,", "nested 3x3")

// test: nested with accumulator
function counter_nested_sum()
    total = 0
    for i = 1, 3
        for j = 1, 3
            total = total + i * j
    assert_equal(total, 36, "nested sum i*j")

// test: nested multiplication table
function counter_nested_mult()
    result = ""
    for i = 1, 3
        for j = 1, 3
            result = "{result}{i * j},"
    assert_equal(result, "1,2,3,2,4,6,3,6,9,", "nested mult table")

// test: nested with break inner
function counter_nested_break_inner()
    result = ""
    for i = 1, 3
        for j = 1, 3
            if j == 2
                break
            result = "{result}{i}{j},"
    assert_equal(result, "11,21,31,", "nested break at j=2")

// test: nested with continue inner
function counter_nested_continue_inner()
    result = ""
    for i = 1, 2
        for j = 1, 3
            if j == 2
                continue
            result = "{result}{i}{j},"
    assert_equal(result, "11,13,21,23,", "nested continue at j=2")

// test: nested with both break via flag
function counter_nested_both_break()
    result = ""
    done = false
    for i = 1, 3
        if done == true
            break
        for j = 1, 3
            if i == 2
                if j == 2
                    done = true
                    break
            result = "{result}{i}{j},"
    assert_equal(result, "11,12,13,21,", "nested break both via flag")

// test: three levels
function counter_nested_3_levels()
    result = ""
    for i = 1, 2
        for j = 1, 2
            for k = 1, 2
                result = "{result}{i}{j}{k},"
    assert_equal(result, "111,112,121,122,211,212,221,222,", "three levels 2x2x2")

// test: nested with different steps
function counter_nested_steps()
    result = ""
    for i = 0, 4, 2
        for j = 0, 3, 3
            result = "{result}{i}-{j},"
    assert_equal(result, "0-0,0-3,2-0,2-3,4-0,4-3,", "nested different steps")

// test: nested reverse outer
function counter_nested_reverse()
    result = ""
    for i = 3, 1, -1
        for j = 1, 2
            result = "{result}{i}{j},"
    assert_equal(result, "31,32,21,22,11,12,", "nested reverse outer")

// test: nested with 4x4 total
function counter_nested_4x4()
    result = ""
    for i = 1, 4
        for j = 1, 4
            result = "{result}{i}{j},"
    assert_equal(result, "11,12,13,14,21,22,23,24,31,32,33,34,41,42,43,44,", "nested 4x4")

// test: nested accumulator with strings
function counter_nested_string()
    result = ""
    for i = 1, 2
        for j = 1, 2
            result = "{result}({i},{j})"
    assert_equal(result, "(1,1)(1,2)(2,1)(2,2)", "nested string build")

// test: triangle with stars
function counter_triangle()
    result = ""
    for i = 1, 4
        for j = 1, i
            result = "{result}*"
        result = "{result}\n"
    assert_equal(result, "*\n**\n***\n****\n", "triangle 4 rows")

// test: reverse triangle
function counter_reverse_triangle()
    result = ""
    for i = 4, 1, -1
        for j = 1, i
            result = "{result}*"
        result = "{result}\n"
    assert_equal(result, "****\n***\n**\n*\n", "reverse triangle")

// test: number pyramid
function counter_pyramid()
    result = ""
    for i = 1, 3
        for j = 1, i
            result = "{result}{j}"
        result = "{result}\n"
    assert_equal(result, "1\n12\n123\n", "number pyramid")

// test: staircase pattern
function counter_staircase()
    result = ""
    for i = 1, 3
        for j = 1, i
            result = "{result}#"
        result = "{result} "
    assert_equal(result, "# ## ### ", "staircase")

// test: alternating sign pattern
function counter_alt_sign()
    result = ""
    sign = 1
    for i = 1, 6
        result = "{result}{i * sign},"
        sign = sign * -1
    assert_equal(result, "1,-2,3,-4,5,-6,", "alternating sign")

// test: box pattern
function counter_box()
    result = ""
    for i = 1, 3
        for j = 1, 3
            result = "{result}*"
        result = "{result}\n"
    assert_equal(result, "***\n***\n***\n", "box 3x3")

// test: zigzag pattern
function counter_zigzag()
    result = ""
    value = 1
    direction = 1
    for i = 1, 5
        result = "{result}{value},"
        if value == 3
            direction = -1
        if value == 1 and direction == -1
            direction = 1
        value = value + direction
    assert_equal(result, "1,2,3,2,1,", "zigzag pattern")

// test: multiplication row
function counter_mult_row()
    result = ""
    for i = 1, 5
        result = "{result}{i * 7},"
    assert_equal(result, "7,14,21,28,35,", "multiplication by 7")

// test: accumulate into table
function counter_table_accum()
    state = ["sum" = 0, "count" = 0]
    for i = 1, 5
        state["sum"] = state["sum"] + i
        state["count"] = state["count"] + 1
    assert_equal(state["sum"], 15, "table sum")
    assert_equal(state["count"], 5, "table count")

// test: table with conditional add
function counter_table_conditional()
    state = ["total" = 0]
    for i = 1, 10
        if i % 2 == 0
            state["total"] = state["total"] + i
    assert_equal(state["total"], 30, "table sum evens")

// test: read from values table
function counter_table_read()
    values = [10, 20, 30, 40, 50]
    sum = 0
    for i = 1, 5
        sum = sum + values[i]
    assert_equal(sum, 150, "sum values table")

// test: find in table
function counter_table_find()
    values = [3, 7, 2, 9, 5]
    target = 9
    found = false
    for i = 1, 5
        if values[i] == target
            found = true
            break
    assert_equal(found, true, "found 9 in table")

// test: count matches in table
function counter_table_count()
    values = [1, 2, 1, 3, 1, 4]
    count = 0
    for i = 1, 6
        if values[i] == 1
            count = count + 1
    assert_equal(count, 3, "count 1s in table")

// test: table with multiple keys
function counter_table_multi_keys()
    state = ["a" = 0, "b" = 0, "c" = 0]
    for i = 1, 9
        if i <= 3
            state["a"] = state["a"] + i
        else if i <= 6
            state["b"] = state["b"] + i
        else
            state["c"] = state["c"] + i
    assert_equal(state["a"], 6, "sum 1-3")
    assert_equal(state["b"], 15, "sum 4-6")
    assert_equal(state["c"], 24, "sum 7-9")

// test: reset table counter
function counter_table_reset()
    state = ["count" = 0]
    for i = 1, 10
        state["count"] = state["count"] + 1
        if state["count"] >= 5
            break
    assert_equal(state["count"], 5, "count reached 5")

// test: table as accumulator with values
function counter_table_values()
    state = ["pos" = 0]
    values = [5, 10, 15, 20]
    sum = 0
    for state["pos"] = 0, 3
        sum = sum + values[state["pos"] + 1]
    assert_equal(sum, 50, "sum via table index")

// test: table read-modify-write
function counter_table_rmw()
    state = ["value" = 100]
    for i = 1, 3
        state["value"] = state["value"] - 10
    assert_equal(state["value"], 70, "repeated subtract")

// test: fizzbuzz style
function counter_fizzbuzz()
    result = ""
    for i = 1, 15
        if i % 15 == 0
            result = "{result}FB,"
        else if i % 3 == 0
            result = "{result}F,"
        else if i % 5 == 0
            result = "{result}B,"
        else
            result = "{result}{i},"
    assert_equal(result, "1,2,F,4,B,F,7,8,F,B,11,F,13,14,FB,", "fizzbuzz 1-15")

// test: multiples of 3
function counter_mult_3()
    result = ""
    for i = 1, 12
        if i % 3 == 0
            result = "{result}{i},"
    assert_equal(result, "3,6,9,12,", "multiples of 3")

// test: multiples of 7
function counter_mult_7()
    result = ""
    for i = 1, 30
        if i % 7 == 0
            result = "{result}{i},"
    assert_equal(result, "7,14,21,28,", "multiples of 7")

// test: divisible by 2 and 3
function counter_div_2_and_3()
    result = ""
    for i = 1, 12
        if i % 2 == 0 and i % 3 == 0
            result = "{result}{i},"
    assert_equal(result, "6,12,", "divisible by 2 and 3")

// test: modulo in condition
function counter_mod_condition()
    result = ""
    for i = 0, 20, 5
        if i % 10 == 0
            result = "{result}{i},"
    assert_equal(result, "0,10,20,", "mod 10 of multiples of 5")

// test: sum multiples of 4
function counter_sum_mult_4()
    sum = 0
    for i = 1, 20
        if i % 4 == 0
            sum = sum + i
    assert_equal(sum, 60, "sum multiples of 4 <= 20")

// test: count divisible
function counter_count_div()
    count = 0
    for i = 1, 100
        if i % 5 == 0
            count = count + 1
    assert_equal(count, 20, "count multiples of 5")

// test: consecutive modulo pattern
function counter_mod_pattern()
    result = ""
    for i = 1, 10
        result = "{result}{i % 3},"
    assert_equal(result, "1,2,0,1,2,0,1,2,0,1,", "mod 3 pattern")

// helper: square
function counter_sq(x)
    return x * x

// helper: is even
function counter_is_even(x)
    return x % 2 == 0

// helper: find divisor
function counter_find_div(start, divisor)
    for i = start, 100
        if i % divisor == 0
            return i
    return -1

// helper: count to n
function counter_count(n)
    total = 0
    for i = 1, n
        total = total + 1
    return total

// test: loop inside function
function counter_loop_in_fn()
    assert_equal(counter_count(5), 5, "count via helper")

// test: function call in body
function counter_fn_call_body()
    sum = 0
    for i = 1, 4
        sum = sum + counter_sq(i)
    assert_equal(sum, 30, "sum squares via fn")

// test: function as predicate
function counter_fn_predicate()
    count = 0
    for i = 1, 10
        if counter_is_even(i) == true
            count = count + 1
    assert_equal(count, 5, "count evens via predicate")

// test: return from inside loop
function counter_return_inside()
    assert_equal(counter_find_div(5, 3), 6, "find first divisible by 3")

// test: function in nested loop
function counter_fn_nested()
    total = 0
    for i = 1, 3
        for j = 1, 3
            total = total + counter_sq(i * j)
    assert_equal(total, 101, "sum squares of products")

// test: loop over function results
function counter_fn_results()
    result = ""
    for i = 1, 5
        result = "{result}{counter_sq(i)},"
    assert_equal(result, "1,4,9,16,25,", "squares via fn")

// test: function with loop parameter
function counter_fn_loop_param()
    assert_equal(counter_count(10), 10, "count to 10")

// test: helper called with loop var
function counter_fn_loop_var()
    sum = 0
    for i = 1, 5
        sum = sum + counter_sq(i)
    assert_equal(sum, 55, "sum of squares 1-5")

// test: if-else in loop
function counter_if_else()
    result = ""
    for i = 1, 5
        if i <= 2
            result = "{result}A"
        else
            result = "{result}B"
    assert_equal(result, "AABBB", "if-else A/B")

// test: else-if chain in loop
function counter_else_if()
    result = ""
    for i = 1, 6
        if i <= 2
            result = "{result}L"
        else if i <= 4
            result = "{result}M"
        else
            result = "{result}H"
    assert_equal(result, "LLMMHH", "else-if L/M/H")

// test: ternary in loop
function counter_ternary()
    result = ""
    for i = 1, 4
        label = "e" if i % 2 == 0 else "o"
        result = "{result}{label}"
    assert_equal(result, "oeoe", "ternary even/odd")

// test: match in loop
function counter_match()
    result = ""
    for i = 1, 4
        match i
            case 1
                result = "{result}one,"
            case 2
                result = "{result}two,"
            case 3
                result = "{result}three,"
            case
                result = "{result}many,"
    assert_equal(result, "one,two,three,many,", "match on i")

// test: nested if in loop
function counter_nested_if()
    result = ""
    for i = 1, 6
        if i % 2 == 0
            if i <= 3
                result = "{result}SE,"
            else
                result = "{result}LE,"
        else
            if i <= 3
                result = "{result}SO,"
            else
                result = "{result}LO,"
    assert_equal(result, "SO,SE,SO,LE,LO,LE,", "nested if even/odd")

// test: multiple branches in loop
function counter_multi_branch()
    result = ""
    for i = 1, 5
        if i == 1
            result = "{result}A"
        else if i == 2
            result = "{result}B"
        else if i == 3
            result = "{result}C"
        else
            result = "{result}D"
    assert_equal(result, "ABCDD", "multi branch")

// test: match with break
function counter_match_break()
    result = ""
    for i = 1, 5
        match i
            case 4
                break
            case
                result = "{result}{i}"
    assert_equal(result, "123", "match break at 4")

// test: match with continue
function counter_match_continue()
    result = ""
    for i = 1, 5
        match i
            case 3
                continue
            case
                result = "{result}{i}"
    assert_equal(result, "1245", "match continue at 3")

// test: condition depends on loop var
function counter_condition_var()
    result = ""
    for i = 1, 10
        if i < 4
            result = "{result}{i}"
    assert_equal(result, "123", "condition i < 4")

// test: accumulated if
function counter_accum_if()
    total = 0
    for i = 1, 10
        if i % 2 == 0
            total = total + i * 2
    assert_equal(total, 60, "sum 2x evens")

// test: fibonacci up to 20
function counter_fibonacci()
    a = 0
    b = 1
    result = ""
    for i = 1, 8
        result = "{result}{a},"
        next = a + b
        a = b
        b = next
    assert_equal(result, "0,1,1,2,3,5,8,13,", "fibonacci first 8")

// test: prime check
function counter_prime_check(n)
    if n < 2
        return false
    for i = 2, n - 1
        if n % i == 0
            return false
    return true

// test: primes up to 20
function counter_primes_20()
    result = ""
    for i = 2, 20
        if counter_prime_check(i) == true
            result = "{result}{i},"
    assert_equal(result, "2,3,5,7,11,13,17,19,", "primes up to 20")

// test: count digits
function counter_digit_count(n)
    if n == 0
        return 1
    count = 0
    value = n
    for i = 1, 100
        if value <= 0
            break
        value = value / 10
        value = value - value % 1
        count = count + 1
    return count

// test: digit count on 12345
function counter_digit_count_test()
    assert_equal(counter_digit_count(12345), 5, "digits of 12345")

// test: sum of digits
function counter_digit_sum(n)
    sum = 0
    value = n
    for i = 1, 100
        if value <= 0
            break
        digit = value % 10
        sum = sum + digit
        value = value / 10
        value = value - value % 1
    return sum

// test: digit sum on 12345
function counter_digit_sum_test()
    assert_equal(counter_digit_sum(12345), 15, "digit sum 1+2+3+4+5")

// test: collatz step count
function counter_collatz_steps(n)
    count = 0
    value = n
    for i = 1, 1000
        if value <= 1
            break
        if value % 2 == 0
            value = value / 2
        else
            value = value * 3 + 1
        count = count + 1
    return count

// test: collatz from 6
function counter_collatz_6()
    assert_equal(counter_collatz_steps(6), 8, "collatz steps from 6")

// test: binary representation length
function counter_binary_len(n)
    if n == 0
        return 1
    count = 0
    value = n
    for i = 1, 100
        if value <= 0
            break
        value = value / 2
        value = value - value % 1
        count = count + 1
    return count

// test: binary length of 8
function counter_binary_len_8()
    assert_equal(counter_binary_len(8), 4, "binary len of 8")

// test: powers of two table
function counter_powers_2()
    result = ""
    for i = 0, 6
        power = 1
        for j = 1, i
            power = power * 2
        result = "{result}{power},"
    assert_equal(result, "1,2,4,8,16,32,64,", "powers of 2")

// test: reverse a table of digits
function counter_reverse_digits()
    digits = [1, 2, 3, 4, 5]
    reversed = ""
    for i = 5, 1, -1
        reversed = "{reversed}{digits[i]}"
    assert_equal(reversed, "54321", "reverse digits")

// test: gcd via euclidean algorithm
function counter_gcd(a, b)
    x = a
    y = b
    for i = 1, 100
        if y == 0
            break
        temp = y
        y = x % y
        x = temp
    return x

// test: gcd of 48 and 18
function counter_gcd_test()
    assert_equal(counter_gcd(48, 18), 6, "gcd 48,18")

// test: lcm using gcd
function counter_lcm(a, b)
    return (a * b) / counter_gcd(a, b)

// test: lcm of 4 and 6
function counter_lcm_test()
    assert_equal(counter_lcm(4, 6), 12, "lcm 4,6")

// test: triangle area accumulation
function counter_triangle_areas()
    sum = 0
    for i = 1, 5
        sum = sum + i * (i + 1) / 2
    assert_equal(sum, 35, "sum triangular numbers")

// test: harmonic-ish sum
function counter_harmonic_sum()
    sum = 0
    for i = 1, 4
        sum = sum + 1 / i
    assert_equal(sum, 2.083333, "harmonic 1..4")

// execute all counter for loop test cases
counter_forward_1_5()
counter_forward_1_1()
counter_forward_0_0()
counter_forward_1_2()
counter_forward_1_3()
counter_forward_1_10()
counter_forward_0_4()
counter_forward_3_7()
counter_forward_100_103()
counter_forward_same()
counter_backward_5_1()
counter_backward_single()
counter_backward_10_1()
counter_backward_5_2()
counter_backward_100_97()
counter_backward_zero()
counter_backward_2_0()
counter_backward_neg()
counter_step_2()
counter_step_2_odd()
counter_step_3()
counter_step_5()
counter_step_10()
counter_step_overshoot()
counter_step_exact()
counter_step_almost()
counter_step_2_start5()
counter_step_neg_2()
counter_step_neg_2_odd()
counter_step_neg_3()
counter_step_neg_5()
counter_step_neg_overshoot()
counter_step_neg_exact()
counter_neg_forward()
counter_neg_backward()
counter_cross_zero()
counter_cross_zero_back()
counter_neg_pos_step()
counter_pos_neg_step()
counter_neg_with_step()
counter_neg_large()
counter_step_half()
counter_step_half_1_3()
counter_step_half_0_1()
counter_step_neg_half()
counter_empty_forward()
counter_empty_backward()
counter_empty_pos_step()
counter_empty_neg_step()
counter_zero_range_step()
counter_single_step()
counter_large_range()
counter_many_iterations()
counter_break_first()
counter_break_middle()
counter_break_last()
counter_break_before_last()
counter_break_neg()
counter_break_step2()
counter_break_if_else()
counter_break_threshold()
counter_break_deep()
counter_break_in_match()
counter_continue_first()
counter_continue_middle()
counter_continue_last()
counter_continue_even()
counter_continue_odd()
counter_continue_mult3()
counter_continue_mult5()
counter_continue_step2()
counter_continue_accum()
counter_continue_in_match()
counter_sum_1_5()
counter_sum_1_10()
counter_sum_1_100()
counter_sum_0_9()
counter_sum_even()
counter_sum_odd()
counter_sum_squares()
counter_sum_cubes()
counter_count_evens()
counter_count_odds()
counter_double_sum()
counter_alternating_sum()
counter_geom_sum()
counter_count_positive()
counter_find_max()
counter_build_123()
counter_build_sep()
counter_build_trailing()
counter_build_prefix()
counter_build_star()
counter_build_chars()
counter_build_lookup()
counter_build_conditional()
counter_build_reverse()
counter_build_csv()
counter_nested_2_2()
counter_nested_3_2()
counter_nested_2_3()
counter_nested_3_3()
counter_nested_sum()
counter_nested_mult()
counter_nested_break_inner()
counter_nested_continue_inner()
counter_nested_both_break()
counter_nested_3_levels()
counter_nested_steps()
counter_nested_reverse()
counter_nested_4x4()
counter_nested_string()
counter_triangle()
counter_reverse_triangle()
counter_pyramid()
counter_staircase()
counter_alt_sign()
counter_box()
counter_zigzag()
counter_mult_row()
counter_table_accum()
counter_table_conditional()
counter_table_read()
counter_table_find()
counter_table_count()
counter_table_multi_keys()
counter_table_reset()
counter_table_values()
counter_table_rmw()
counter_fizzbuzz()
counter_mult_3()
counter_mult_7()
counter_div_2_and_3()
counter_mod_condition()
counter_sum_mult_4()
counter_count_div()
counter_mod_pattern()
counter_loop_in_fn()
counter_fn_call_body()
counter_fn_predicate()
counter_return_inside()
counter_fn_nested()
counter_fn_results()
counter_fn_loop_param()
counter_fn_loop_var()
counter_if_else()
counter_else_if()
counter_ternary()
counter_match()
counter_nested_if()
counter_multi_branch()
counter_match_break()
counter_match_continue()
counter_condition_var()
counter_accum_if()
counter_fibonacci()
counter_primes_20()
counter_digit_count_test()
counter_digit_sum_test()
counter_collatz_6()
counter_binary_len_8()
counter_powers_2()
counter_reverse_digits()
counter_gcd_test()
counter_lcm_test()
counter_triangle_areas()
counter_harmonic_sum()

// report completion of counter for loop tests
os.output("[PASS] loops/for_counter.apex")