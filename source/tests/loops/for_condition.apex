import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function for_basic_count_up()
    result = ""
    counter = 1
    for counter <= 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "basic count up 1 to 5")

function for_count_down()
    result = ""
    counter = 5
    for counter >= 1
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "54321", "count down 5 to 1")

function for_with_step()
    result = ""
    counter = 0
    for counter <= 10
        result = "{result}{counter}"
        counter = counter + 2
    assert_equal(result, "0246810", "step 2, 0 to 10")

function for_with_break()
    result = ""
    counter = 1
    for counter <= 5
        if counter == 3
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12", "break at 3")

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

function for_single()
    result = ""
    counter = 1
    for counter <= 1
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1", "single iteration")

function for_zero()
    result = ""
    counter = 1
    for counter > 5
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "", "zero iterations")

function for_accumulator()
    sum = 0
    counter = 1
    for counter <= 5
        sum = sum + counter
        counter = counter + 1
    assert_equal(sum, 15, "sum 1 to 5")

function for_negative()
    result = ""
    counter = -5
    for counter <= -1
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "-5-4-3-2-1", "negative count up")

function for_negative_decrement()
    result = ""
    counter = -1
    for counter >= -5
        result = "{result}{counter}"
        counter = counter - 1
    assert_equal(result, "-1-2-3-4-5", "negative count down")

function for_doubling()
    result = ""
    counter = 1
    for counter <= 16
        result = "{result}{counter}"
        counter = counter * 2
    assert_equal(result, "124816", "doubling 1 to 16")

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

if tests_failed == 0
    os.output("[OK] loops/for_condition.apex")