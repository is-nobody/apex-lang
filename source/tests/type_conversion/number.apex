import os

function number_from_int_string()
    result = number("42")
    os.output("number("42") = {result}, expected 42")

function number_from_decimal_string()
    result = number("3.14")
    os.output("number("3.14") = {result}, expected 3.14")

function number_from_true()
    result = number(true)
    os.output("number(true) = {result}, expected 1")

function number_from_false()
    result = number(false)
    os.output("number(false) = {result}, expected 0")

function number_from_invalid_string()
    result = number("hello")
    os.output("number("hello") = {result}, expected false")

function number_from_negative_string()
    result = number("-5")
    os.output("number("-5") = {result}, expected -5")

function number_from_zero_string()
    result = number("0")
    os.output("number("0") = {result}, expected 0")

number_from_int_string()
number_from_decimal_string()
number_from_true()
number_from_false()
number_from_invalid_string()
number_from_negative_string()
number_from_zero_string()