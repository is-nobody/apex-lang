import os

function for_inside_for()
    for i = 1, 5
        line = ""
        for j = 1, 5
            line = "{line}{i} × {j} = {i*j}\t"
        os.output(line)

function for_inside_while()
    i = 1
    while i <= 5
        line = ""
        for j = 1, 5
            line = "{line}{i} × {j} = {i*j}\t"
        os.output(line)
        i = i + 1

function while_inside_for()
    for i = 1, 5
        line = ""
        j = 1
        while j <= 5
            line = "{line}{i} × {j} = {i*j}\t"
            j = j + 1
        os.output(line)

function while_inside_while()
    i = 1
    while i <= 5
        line = ""
        j = 1
        while j <= 5
            line = "{line}{i} × {j} = {i*j}\t"
            j = j + 1
        os.output(line)
        i = i + 1

for_inside_for()
for_inside_while()
while_inside_for()
while_inside_while()