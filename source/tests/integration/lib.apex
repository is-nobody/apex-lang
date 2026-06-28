import os
import math

pi = math.pi()
e = math.e()

function add(a, b)
    return a + b

function subtract(a, b)
    return a - b

function multiply(a, b)
    return a * b

function divide(a, b)
    if b == 0
        os.output("Error: Division by zero")
        return 0
    return a / b

function power(base, exp)
    result = 1
    i = 0
    for i = 1, exp
        result = result * base
    return result

function factorial(n)
    if n <= 1
        return 1
    return n * factorial(n - 1)

function circle_area(radius)
    return pi * radius * radius