import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function inf_basic_break()
    result = ""
    counter = 1
    for
        if counter > 5
            break
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "12345", "basic infinite with break")

function inf_continue()
    result = ""
    counter = 1
    for
        if counter > 5
            break
        if counter == 3
            counter = counter + 1
            continue
        result = "{result}{counter}"
        counter = counter + 1
    assert_equal(result, "1245", "infinite skip 3")

function inf_accumulator()
    sum = 0
    i = 1
    for
        if i > 5
            break
        sum = sum + i
        i = i + 1
    assert_equal(sum, 15, "infinite accumulator")

function inf_nested()
    result = ""
    i = 1
    for
        if i > 2
            break
        j = 1
        for
            if j > 2
                break
            result = "{result}{i}{j}"
            j = j + 1
        i = i + 1
    assert_equal(result, "11122122", "nested infinite loops")

function inf_do_while_simulation()
    result = ""
    counter = 10
    for
        result = "{result}{counter}"
        counter = counter + 1
        if counter > 10
            break
    assert_equal(result, "10", "do-while simulation")

function inf_multiple_breaks()
    result = ""
    i = 1
    for
        if i == 3
            result = "{result}3"
            break
        if i == 5
            result = "{result}5"
            break
        result = "{result}{i}"
        i = i + 1
    assert_equal(result, "123", "multiple breaks")

inf_basic_break()
inf_continue()
inf_accumulator()
inf_nested()
inf_do_while_simulation()
inf_multiple_breaks()

if tests_failed == 0
    os.output("[OK] loops/for_infinity.apex")