import os

function countdown()
    os.output("Countdown:")
    total = 10

    for sec = total, 0, -1
        display = sec
        if display < 10
            display = "0{display}"
        
        os.output("  {display}...")
        os.wait(1)

    os.output("  🚀 Liftoff!")

countdown()