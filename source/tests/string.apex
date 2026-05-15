import os

// basic output with interpolation
name = "Alice"
age = 30
os.output("Hello, {name}!")
os.output("{name} is {age} years old")

// none in interpolation
x = none
os.output("x is {x}")

// strings with quotes inside
os.output("He said: "Hello!"")

// empty string
os.output("")

// concatenation via interpolation
first = "Hello"
second = "World"
os.output("{first}, {second}!")

// booleans in interpolation
is_active = true
os.output("Status: {is_active}")

// numbers in interpolation
score = 95
price = 19.99
os.output("Score: {score}, Price: {price}")

// tables in interpolation
user = ("Alice", age = 30)
os.output("Name: {user.1}, Age: {user.age}")