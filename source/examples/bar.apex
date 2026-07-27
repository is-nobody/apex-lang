import os

function bar_load()
    total = 50
    width = 30

    for i = 0, total
        filled = (i * width) / total
        bar = ""
        
        for j = 1, width
            if j <= filled
                bar = "{bar}█"
            else
                bar = "{bar}░"
        
        percent = (i * 100) / total
        os.output("\r[{bar}] {percent}%")
        os.wait(0.05)

bar_load()