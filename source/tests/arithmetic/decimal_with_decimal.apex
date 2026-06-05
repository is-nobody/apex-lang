// decimal_with_decimal.apex
import os

function decimal_with_decimal_add()
    result = 3.5 + 1.5
    os.output("3.5 + 1.5 = {result}, expected 5")

function decimal_with_decimal_subtract()
    result = 3.5 - 1.5
    os.output("3.5 - 1.5 = {result}, expected 2")

function decimal_with_decimal_divide()
    result = 3.5 / 1.5
    os.output("3.5 / 1.5 = {result}, expected 2.33333")

function decimal_with_decimal_multiply()
    result = 3.5 * 1.5
    os.output("3.5 * 1.5 = {result}, expected 5.25")

function decimal_with_decimal_modulo()
    result = 3.5 % 1.5
    os.output("3.5 % 1.5 = {result}, expected 0.5")

decimal_with_decimal_add()
decimal_with_decimal_subtract()
decimal_with_decimal_divide()
decimal_with_decimal_multiply()
decimal_with_decimal_modulo()