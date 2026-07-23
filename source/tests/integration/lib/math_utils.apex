import math

function circle_area(radius)
    return math.pi() * radius * radius

function power(base, exp)
    result = 1
    i = 0
    for i = 1, exp
        result = result * base
    return result

function factorial(n)
    result = 1
    for i = 2, n, 1
        result = result * i
    return result

function absolute(x)
    if x < 0
        return -x
    return x

function is_even(n)
    return n % 2 == 0

function is_odd(n)
    return n % 2 != 0