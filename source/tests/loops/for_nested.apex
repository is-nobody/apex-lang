import os

function for_counter_inside_for_counter()
    for i = 1, 5
        line = ""
        for j = 1, 5
            line = "{line}{i} × {j} = {i*j}\t"
        os.output(line)

function for_condition_inside_for_counter()
    i = 1
    for i <= 5
        line = ""
        for j = 1, 5
            line = "{line}{i} × {j} = {i*j}\t"
        os.output(line)
        i = i + 1

function for_infinity()
    counter = 1
    for 
        if counter > 5
            break
        os.output("Counter: {counter}")
        counter = counter + 1
    os.output("Loop finished successfully")

for_counter_inside_for_counter()
for_condition_inside_for_counter()
for_infinity()

for i == 5
    for_infinity()