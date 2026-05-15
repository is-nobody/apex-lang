import os

// empty table
empty = ()
os.output(empty)

// ordered list
fruits = ("apple", "banana", "orange")
os.output(fruits.1)
os.output(fruits.2)
os.output(fruits.3)

// key-value pairs
user = (name = "Alice", age = 30, active = true)
os.output(user.name)
os.output(user.age)
os.output(user.active)

// mixed table
person = ("Alice", "Manager", department = "Engineering", years = 5)
os.output(person.1)
os.output(person.2)
os.output(person.department)
os.output(person.years)

// accessing non-existent key
test_table = (name = "Test")
missing_key = test_table.age
if missing_key == none
    os.output("non-existent key returns none")

// updating existing key
user.age = 31
os.output(user.age)

// adding new key
user.city = "Dubai"
os.output(user.city)

// setting item to none
user.name = none
os.output(user.name)

// accessing index 0
item_zero = fruits.0
if item_zero == none
    os.output("index 0 returns none")
else
    os.output("index 0 not returns none")