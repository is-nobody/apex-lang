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

function output_escape_quote()
    os.output("He said: \"I hate donuts!\"")

function output_interpolation_escaped()
    os.output("\{hi\}")

function output_escape_backslash()
    os.output("Backslash: \\")

function output_newline()
    os.output("first line\nsecond line")

function output_multiline()
    os.output("first line
second line")

function output_tabulation()
    os.output("Name\tAge\tCity\nAlice\t30\tLondon")

function output_table()
    t = [
        name = "config",
        files = [
            main = ["ggg"],
            secondary = "build"
        ]
    ]
    os.output(t)

output_empty()
output_string()
output_number()
output_boolean()
output_interpolation()
output_interpolation_arithmetic()
output_escape_quote()
output_interpolation_escaped()
output_escape_backslash()
output_newline()
output_multiline()
output_tabulation()
output_table()