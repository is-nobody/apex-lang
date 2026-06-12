import os

function inf_basic_break()
    os.output("Basic infinite loop with break:")
    counter = 1
    for
        if counter > 5
            break
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 3 4 5")

function inf_continue()
    os.output("Infinite loop with continue (skip 3):")
    counter = 1
    for
        if counter > 5
            break
        if counter == 3
            counter = counter + 1
            continue
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 4 5")

function inf_accumulator()
    os.output("Infinite loop accumulator:")
    sum = 0
    i = 1
    for
        if i > 5
            break
        sum = sum + i
        i = i + 1
    os.output("Sum = {sum}, expected 15")

function inf_nested()
    os.output("Nested infinite loops:")
    i = 1
    for
        if i > 2
            break
        j = 1
        for
            if j > 2
                break
            os.output("i={i}, j={j}")
            j = j + 1
        i = i + 1
    os.output("Expected: 1,1 1,2 2,1 2,2")

function inf_do_while_simulation()
    os.output("Simulating do-while (execute at least once):")
    counter = 10
    for
        os.output("Counter: {counter}")
        counter = counter + 1
        if counter > 10
            break
    os.output("Expected: Counter: 10")

function inf_multiple_breaks()
    os.output("Multiple break conditions:")
    i = 1
    for
        if i == 3
            os.output("Hit 3, breaking")
            break
        if i == 5
            os.output("Hit 5, breaking")
            break
        os.output("i={i}")
        i = i + 1
    os.output("Expected: i=1 i=2 Hit 3, breaking")

inf_basic_break()
inf_continue()
inf_accumulator()
inf_nested()
inf_do_while_simulation()
inf_multiple_breaks()