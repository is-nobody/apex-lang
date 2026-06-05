// whole_with_decimal.apex
import os

function whole_with_decimal_add()
    result = 5 + 2.5
    os.output("5 + 2.5 = {result}, expected 7.5")

function whole_with_decimal_subtract()
    result = 5 - 2.5
    os.output("5 - 2.5 = {result}, expected 2.5")

function whole_with_decimal_divide()
    result = 5 / 2.5
    os.output("5 / 2.5 = {result}, expected 2.0")

function whole_with_decimal_multiply()
    result = 5 * 2.5
    os.output("5 * 2.5 = {result}, expected 12.5")

function whole_with_decimal_modulo()
    result = 5 % 2.5
    os.output("5 % 2.5 = {result}, expected 0")

whole_with_decimal_add()
whole_with_decimal_subtract()
whole_with_decimal_divide()
whole_with_decimal_multiply()
whole_with_decimal_modulo()