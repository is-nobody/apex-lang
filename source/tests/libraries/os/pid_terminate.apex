import os
import sys

function terminate_process()
    p = sys.process_id()
    os.output("Current process ID is {p}")
    os.output("Goodbye, cruel world")
    os.terminate_process(p)

terminate_process()