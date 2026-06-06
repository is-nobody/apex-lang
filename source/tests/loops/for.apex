import os

function for_basic_forward()
    os.output("Basic forward (1 to 5):")
    for i = 1, 5
        os.output(i)
    os.output("Expected: 1 2 3 4 5")

function for_forward_step()
    os.output("Forward step 2 (0 to 10):")
    for i = 0, 10, 2
        os.output(i)
    os.output("Expected: 0 2 4 6 8 10")

function for_backward()
    os.output("Backward (5 to 1):")
    for i = 5, 1, -1
        os.output(i)
    os.output("Expected: 5 4 3 2 1")

function for_backward_step()
    os.output("Backward step 3 (10 to 1):")
    for i = 10, 1, -3
        os.output(i)
    os.output("Expected: 10 7 4 1")

function for_single()
    os.output("Single iteration (3 to 3):")
    for i = 3, 3
        os.output(i)
    os.output("Expected: 3")

function for_with_break()
    os.output("Break at 3 (1 to 5):")
    for i = 1, 5
        if i == 3
            break
        os.output(i)
    os.output("Expected: 1 2")

function for_with_continue()
    os.output("Skip 3 (1 to 5):")
    for i = 1, 5
        if i == 3
            continue
        os.output(i)
    os.output("Expected: 1 2 4 5")

function for_negative_forward()
    os.output("Negative forward (-5 to -1):")
    for i = -5, -1
        os.output(i)
    os.output("Expected: -5 -4 -3 -2 -1")

function for_negative_backward()
    os.output("Negative backward (-1 to -5):")
    for i = -1, -5, -1
        os.output(i)
    os.output("Expected: -1 -2 -3 -4 -5")

function for_zero_range()
    os.output("Zero range (-2 to 2):")
    for i = -2, 2
        os.output(i)
    os.output("Expected: -2 -1 0 1 2")

function for_large_step()
    os.output("Large step 5 (0 to 20):")
    for i = 0, 20, 5
        os.output(i)
    os.output("Expected: 0 5 10 15 20")

function for_decimal_step()
    os.output("Decimal step 0.5 (0 to 2):")
    for i = 0, 2, 0.5
        os.output(i)
    os.output("Expected: 0 0.5 1.0 1.5 2.0")

function for_accumulator()
    sum = 0
    for i = 1, 5
        sum = sum + i
    os.output("Sum 1 to 5 = {sum}, expected 15")

function for_string_build()
    result = ""
    for i = 1, 3
        result = "{result}{i}"
    os.output("String build = {result}, expected 123")

function for_nested()
    os.output("Nested loops (1-2, 1-2):")
    for i = 1, 2
        for j = 1, 2
            os.output("i={i}, j={j}")
    os.output("Expected: 1,1 1,2 2,1 2,2")

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