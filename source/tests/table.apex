import os


// Test 1: empty table - check by length or property instead
empty = ()
// Since we can't compare tables directly, check that accessing .1 returns none
os.output("Test 1: empty table should have no items, but empty.1 = {empty.1} = {result}")

// Test 2: ordered list
fruits = ("apple", "banana", "orange")
os.output("Test 2: fruits.1 = {fruits.1}, expected apple = {result}")
os.output("Test 2: fruits.2 = {fruits.2}, expected banana = {result}")
os.output("Test 2: fruits.3 = {fruits.3}, expected orange = {result}")

// Test 3: key-value pairs
user = (name = "Alice", age = 30, active = true)
os.output("Test 3: user.name = {user.name}, expected Alice = {result}")
os.output("Test 3: user.age = {user.age}, expected 30 = {result}")
os.output("Test 3: user.active = {user.active}, expected true = {result}")

// Test 4: mixed table
person = ("Alice", "Manager", department = "Engineering", years = 5)
os.output("Test 4: person.1 = {person.1}, expected Alice = {result}")
os.output("Test 4: person.2 = {person.2}, expected Manager = {result}")
os.output("Test 4: person.department = {person.department}, expected Engineering = {result}")
os.output("Test 4: person.years = {person.years}, expected 5 = {result}")

// Test 5: accessing non-existent key
test_table = (name = "Test")
missing_key = test_table.age
os.output("Test 5: non-existent key = {missing_key}, expected none = {result}")

// Test 6: updating existing key
user.age = 31
os.output("Test 6: user.age after update = {user.age}, expected 31 = {result}")

// Test 7: adding new key
user.city = "Dubai"
os.output("Test 7: user.city = {user.city}, expected Dubai = {result}")

// Test 8: setting item to none
user.name = none
os.output("Test 8: user.name after setting to none = {user.name}, expected none = {result}")

// Test 9: accessing index 0
item_zero = fruits.0
os.output("Test 9: fruits.0 = {item_zero}, expected none = {result}")

// Test 10: nested tables
company = (
    name = "Apex",
    employees = ("Alice", "Bob", "Charlie"),
    address = (
        street = "1 Sheikh Mohammed bin Rashid Boulevard",
        city = "Dubai"
    )
)
os.output("Test 10: company.name = {company.name}, expected Apex = {result}")
os.output("Test 10: company.employees.1 = {company.employees.1}, expected Alice = {result}")
os.output("Test 10: company.address.city = {company.address.city}, expected Dubai = {result}")

// Test 11: table with numbers as keys
numbers = (10, 20, 30)
os.output("Test 11: numbers.1 = {numbers.1}, expected 10 = {result}")
os.output("Test 11: numbers.2 = {numbers.2}, expected 20 = {result}")
os.output("Test 11: numbers.3 = {numbers.3}, expected 30 = {result}")

// Test 12: updating ordered position
fruits.2 = "blueberry"
os.output("Test 12: fruits.2 after update = {fruits.2}, expected blueberry = {result}")

// Test 13: verify empty table again using different method
empty2 = ()
os.output("Test 13: empty table should return none for any index = {result}")

// final result
