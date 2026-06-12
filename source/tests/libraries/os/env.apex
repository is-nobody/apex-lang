import os
import table
// Test: os.env
os.output("=== os.env ===")
env = os.env()
os.output("Has USER: {table.has(env, \"USER\")}")
os.output("Has PATH: {table.has(env, \"PATH\")}")
os.output("Has HOME: {table.has(env, \"HOME\")}")
os.output("Total vars: {table.size(env)}")