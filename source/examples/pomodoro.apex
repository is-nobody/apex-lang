import os
import math

function pomodoro()
    work_seconds = 25 * 60
    os.output("Pomodoro started! 25 minutes...")

    for remaining = work_seconds, 0, -1
        mins = math.round_down(remaining / 60)
        secs = remaining % 60
        
        secs_str = secs
        if secs < 10
            secs_str = "0{secs}"
        
        os.output("Time left: {mins}:{secs_str}")
        os.wait(1)

    os.output("Time's up! Take a break!")

pomodoro()