import os

// range as built-in keyword
for i in range(1, 6)
    os.output(i)

for i in range(0, 11, 2)
    os.output(i)

for i in range(5, 0, -1)
    os.output(i)

// Break and continue work normally
for i in range(1, 11)
    if i == 5
        break
    os.output(i)

for i in range(1, 6)
    if i == 3
        continue
    os.output(i)