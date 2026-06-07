import os

function output_empty()
    os.output("")

function output_string()
    os.output("Hello, Friend")

function output_number()
    os.output(42)
    os.output(3.14)
    os.output(0)
    os.output(-10)

function output_boolean()
    os.output(true)
    os.output(false)

function output_interpolation()
    i = "interpolation"
    os.output("This text with {i}")

function output_interpolation_arithmetic()
    five = 5
    os.output("5 * 10 = {five * 10}, expected 50")

function output_with_quotes()
    os.output("He said: "I hate donuts!"")

function output_multiline()
    os.output("
first line
second line
")

output_empty()
output_string()
output_number()
output_boolean()
output_interpolation()
output_interpolation_arithmetic()
output_with_quotes()
output_multiline()