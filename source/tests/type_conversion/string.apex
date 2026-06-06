import os

function string_from_int()
    result = string(42)
    os.output("string(42) = {result}, expected 42")

function string_from_decimal()
    result = string(3.14)
    os.output("string(3.14) = {result}, expected 3.14")

function string_from_true()
    result = string(true)
    os.output("string(true) = {result}, expected true")

function string_from_false()
    result = string(false)
    os.output("string(false) = {result}, expected false")

function string_from_negative()
    result = string(-10)
    os.output("string(-10) = {result}, expected -10")

function string_from_zero()
    result = string(0)
    os.output("string(0) = {result}, expected 0")

string_from_int()
string_from_decimal()
string_from_true()
string_from_false()
string_from_negative()
string_from_zero()