// whole_with_whole.apex
import os

function whole_with_whole_add()
    result = 10 + 3
    os.output("10 + 3 = {result}, expected 13")

function whole_with_whole_subtract()
    result = 10 - 3
    os.output("10 - 3 = {result}, expected 7")

function whole_with_whole_divide()
    result = 10 / 3
    os.output("10 / 3 = {result}, expected 3.33333")

function whole_with_whole_multiply()
    result = 10 * 3
    os.output("10 * 3 = {result}, expected 30")

function whole_with_whole_modulo()
    result = 10 % 3
    os.output("10 % 3 = {result}, expected 1")

whole_with_whole_add()
whole_with_whole_subtract()
whole_with_whole_divide()
whole_with_whole_multiply()
whole_with_whole_modulo()