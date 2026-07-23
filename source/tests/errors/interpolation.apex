import os
import math
import string
import random

os.output("Simple: {undefined_var}")
os.output("Multiple: {a} and {b} and {c}")
os.output("Nested expr: {undefined_var + 1}")
os.output("Function call: {undefined_func()}")

os.output('Abs string: {math.abs("hello")}')
os.output("Abs bool: {math.abs(true)}")
os.output("Sqrt negative: {math.sqrt(-1)}")
os.output('Sub wrong args: {string.sub("hello")}')
os.output('Sub too many: {string.sub("hello", 1, 2, 3)}')
os.output('Floor string: {math.floor("test")}')

os.output("Bad math func: {math.undefined_func(1)}")
os.output('Bad string func: {string.nonexistent("test")}')
os.output("Bad os func: {os.fake_function()}")

os.output('No import: {json.parse("test")}')
os.output('Deep no import: {crypto.hash.sha256("data")}')

os.output('Number add string: {math.abs("hello" + 5)}')
os.output("Bool in math: {math.sin(true)}")
os.output('String in comparison: {1 < "hello"}')

os.output("Bad syntax: {1 + }")
os.output("Unclosed paren: {func(1, 2}")
os.output("Double dot: {math..abs(1)}")

os.output('Mix: {undefined_var} and {math.abs("test")} and {string.bad()}')
os.output('Deep: {math.abs(math.floor("test"))}')

os.output("Empty: {}")
os.output("Just number: {42}")
os.output('Just string: {"hello"}')
os.output("Escaped: \\{not_interpolation}")
os.output("Nested strings: {'test \"nested\" here'}")
os.output('Complex: {table["key"] + math.abs(-5)}')

os.output("Random range: {random.randint(1)}")
os.output("Random range rev: {random.randint(10, 1)}")
os.output("File op: {os.read(123)}")
os.output('Wait string: {os.wait("forever")}')

os.output('Nested call: {string.length(math.abs("test"))}')
os.output("Chain error: {math.floor(string.length(123))}")