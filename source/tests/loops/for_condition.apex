import os

function for_basic_count_up()
    os.output("Basic count up (1 to 5):")
    counter = 1
    for counter <= 5
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 3 4 5")

function for_count_down()
    os.output("Count down (5 to 1):")
    counter = 5
    for counter >= 1
        os.output(counter)
        counter = counter - 1
    os.output("Expected: 5 4 3 2 1")

function for_with_step()
    os.output("Step 2 (0 to 10):")
    counter = 0
    for counter <= 10
        os.output(counter)
        counter = counter + 2
    os.output("Expected: 0 2 4 6 8 10")

function for_with_break()
    os.output("Break at 3 (1 to 5):")
    counter = 1
    for counter <= 5
        if counter == 3
            break
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2")

function for_with_continue()
    os.output("Skip 3 (1 to 5):")
    counter = 1
    for counter <= 5
        if counter == 3
            counter = counter + 1
            continue
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 4 5")

function for_single()
    os.output("Single iteration (1 to 1):")
    counter = 1
    for counter <= 1
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1")

function for_zero()
    os.output("Zero iterations (false condition):")
    counter = 1
    for counter > 5
        os.output(counter)
        counter = counter + 1
    os.output("Expected: (no output)")

function for_accumulator()
    os.output("Sum with for (1 to 5):")
    sum = 0
    counter = 1
    for counter <= 5
        sum = sum + counter
        counter = counter + 1
    os.output("Sum = {sum}, expected 15")

function for_negative()
    os.output("Negative count up (-5 to -1):")
    counter = -5
    for counter <= -1
        os.output(counter)
        counter = counter + 1
    os.output("Expected: -5 -4 -3 -2 -1")

function for_negative_decrement()
    os.output("Negative count down (-1 to -5):")
    counter = -1
    for counter >= -5
        os.output(counter)
        counter = counter - 1
    os.output("Expected: -1 -2 -3 -4 -5")

function for_doubling()
    os.output("Doubling (1 to 16):")
    counter = 1
    for counter <= 16
        os.output(counter)
        counter = counter * 2
    os.output("Expected: 1 2 4 8 16")

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