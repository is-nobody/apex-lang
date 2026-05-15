import os

function greet(name)
    os.output("Hello, {name}!")

greet("Alice")
greet("Bob")

function add(a, b)
    return a + b

result = add(5, 3)
os.output("5 + 3 = {result}")

function check_age(age)
    if age < 18
        return "Too young"
    return "Welcome"

os.output(check_age(15))
os.output(check_age(25))