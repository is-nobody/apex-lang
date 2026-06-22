import os

function terminate_process()
    p = os.current_process_id()
    os.output("Current PID is {p}")
    os.output("Goodbye, cruel world")
    os.terminate_process(p)

terminate_process()