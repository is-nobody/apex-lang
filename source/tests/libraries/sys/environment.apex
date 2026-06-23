import os
import table

function environment()
    e = sys.environment()
    os.output("Has USER: {table.has(e, \"USER\")}")
    os.output("Has PATH: {table.has(e, \"PATH\")}")
    os.output("Has HOME: {table.has(e, \"HOME\")}")
    os.output("Total vars: {table.size(e)}")

environment()