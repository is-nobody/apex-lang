// math_lib.apex - Module with functions and variables
import os

pi = 3.14159
e = 2.71828

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
    for i in range(exp)
        result = result * base
    return result

function factorial(n)
    if n <= 1
        return 1
    return n * factorial(n - 1)

function circle_area(radius)
    return pi * radius * radius