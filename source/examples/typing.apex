import os
import string

function typing_print()
    text = "Hello, I am Apex..."
    delay = 0.05

    for i = 1, string.length(text)
        line = string.slice(text, 0, i)
        os.execute("clear")
        os.output(line)
        os.wait(delay)

typing_print()