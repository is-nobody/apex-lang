// source/tests/errors/interpolation.apex
// Implementation of String Interpolation Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import math
import string
import random

// undefined variable errors
os.output("Simple: {undefined_var}")
os.output("Multiple: {a} and {b} and {c}")
os.output("Nested expr: {undefined_var + 1}")
os.output("Function call: {undefined_func()}")

// invalid argument type errors
os.output('Abs string: {math.abs("hello")}')
os.output("Abs bool: {math.abs(true)}")
os.output("Sqrt negative: {math.sqrt(-1)}")
os.output('Sub wrong args: {string.slice("hello")}')
os.output('Sub too many: {string.slice("hello", 1, 2, 3)}')
os.output('Floor string: {math.round_down("test")}')

// undefined function errors
os.output("Bad math func: {math.undefined_func(1)}")
os.output('Bad string func: {string.nonexistent("test")}')
os.output("Bad os func: {os.fake_function()}")

// missing import errors
os.output('No import: {json.parse("test")}')
os.output('Deep no import: {crypto.hash.sha256("data")}')

// type mismatch errors
os.output('Number add string: {math.abs("hello" + 5)}')
os.output("Bool in math: {math.sin(true)}")
os.output('String in comparison: {1 < "hello"}')

// syntax errors within interpolation
os.output("Bad syntax: {1 + }")
os.output("Unclosed paren: {func(1, 2}")
os.output("Double dot: {math..abs(1)}")

// mixed errors
os.output('Mix: {undefined_var} and {math.abs("test")} and {string.bad()}')
os.output('Deep: {math.abs(math.round_down("test"))}')

// edge cases - valid but unusual
os.output("Empty: {}")
os.output("Just number: {42}")
os.output('Just string: {"hello"}')
os.output("Escaped: \\{not_interpolation}")
os.output("Nested strings: {'test \"nested\" here'}")
os.output('Complex: {table["key"] + math.abs(-5)}')

// library invalid argument errors
os.output("Random range: {random.integer(1)}")
os.output("Random range rev: {random.integer(10, 1)}")
os.output("File op: {os.read(123)}")
os.output('Wait string: {os.wait("forever")}')

// nested error propagation
os.output('Nested call: {string.length(math.abs("test"))}')
os.output("Chain error: {math.round_down(string.length(123))}")