// while_loop.apex
import os

// Basic while counting up
function while_basic_count_up()
    os.output("Basic count up (1 to 5):")
    counter = 1
    while counter <= 5
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 3 4 5")

// While counting down
function while_count_down()
    os.output("Count down (5 to 1):")
    counter = 5
    while counter >= 1
        os.output(counter)
        counter = counter - 1
    os.output("Expected: 5 4 3 2 1")

// While with step
function while_with_step()
    os.output("Step 2 (0 to 10):")
    counter = 0
    while counter <= 10
        os.output(counter)
        counter = counter + 2
    os.output("Expected: 0 2 4 6 8 10")

// While with break
function while_with_break()
    os.output("Break at 3 (1 to 5):")
    counter = 1
    while counter <= 5
        if counter == 3
            break
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2")

// While with continue
function while_with_continue()
    os.output("Skip 3 (1 to 5):")
    counter = 1
    while counter <= 5
        if counter == 3
            counter = counter + 1
            continue
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1 2 4 5")

// While single iteration
function while_single()
    os.output("Single iteration (1 to 1):")
    counter = 1
    while counter <= 1
        os.output(counter)
        counter = counter + 1
    os.output("Expected: 1")

// While zero iterations
function while_zero()
    os.output("Zero iterations (false condition):")
    counter = 1
    while counter > 5
        os.output(counter)
        counter = counter + 1
    os.output("Expected: (no output)")

// While accumulator
function while_accumulator()
    os.output("Sum with while (1 to 5):")
    sum = 0
    counter = 1
    while counter <= 5
        sum = sum + counter
        counter = counter + 1
    os.output("Sum = {sum}, expected 15")

// While with negative numbers
function while_negative()
    os.output("Negative count up (-5 to -1):")
    counter = -5
    while counter <= -1
        os.output(counter)
        counter = counter + 1
    os.output("Expected: -5 -4 -3 -2 -1")

// While decrementing negative
function while_negative_decrement()
    os.output("Negative count down (-1 to -5):")
    counter = -1
    while counter >= -5
        os.output(counter)
        counter = counter - 1
    os.output("Expected: -1 -2 -3 -4 -5")

// While with multiplication
function while_doubling()
    os.output("Doubling (1 to 16):")
    counter = 1
    while counter <= 16
        os.output(counter)
        counter = counter * 2
    os.output("Expected: 1 2 4 8 16")

while_basic_count_up()
while_count_down()
while_with_step()
while_with_break()
while_with_continue()
while_single()
while_zero()
while_accumulator()
while_negative()
while_negative_decrement()
while_doubling()