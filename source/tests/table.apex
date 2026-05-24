import os

errors = 0

// Test 1: empty table - check by length or property instead
empty = ()
// Since we can't compare tables directly, check that accessing .1 returns none
if empty.1 != none
    errors = errors + 1
    os.output("  ERROR Test 1: empty table should have no items, but empty.1 = {empty.1}")

// Test 2: ordered list
fruits = ("apple", "banana", "orange")
if fruits.1 != "apple"
    errors = errors + 1
    os.output("  ERROR Test 2: fruits.1 = {fruits.1}, expected apple")
if fruits.2 != "banana"
    errors = errors + 1
    os.output("  ERROR Test 2: fruits.2 = {fruits.2}, expected banana")
if fruits.3 != "orange"
    errors = errors + 1
    os.output("  ERROR Test 2: fruits.3 = {fruits.3}, expected orange")

// Test 3: key-value pairs
user = (name = "Alice", age = 30, active = true)
if user.name != "Alice"
    errors = errors + 1
    os.output("  ERROR Test 3: user.name = {user.name}, expected Alice")
if user.age != 30
    errors = errors + 1
    os.output("  ERROR Test 3: user.age = {user.age}, expected 30")
if user.active != true
    errors = errors + 1
    os.output("  ERROR Test 3: user.active = {user.active}, expected true")

// Test 4: mixed table
person = ("Alice", "Manager", department = "Engineering", years = 5)
if person.1 != "Alice"
    errors = errors + 1
    os.output("  ERROR Test 4: person.1 = {person.1}, expected Alice")
if person.2 != "Manager"
    errors = errors + 1
    os.output("  ERROR Test 4: person.2 = {person.2}, expected Manager")
if person.department != "Engineering"
    errors = errors + 1
    os.output("  ERROR Test 4: person.department = {person.department}, expected Engineering")
if person.years != 5
    errors = errors + 1
    os.output("  ERROR Test 4: person.years = {person.years}, expected 5")

// Test 5: accessing non-existent key
test_table = (name = "Test")
missing_key = test_table.age
if missing_key != none
    errors = errors + 1
    os.output("  ERROR Test 5: non-existent key = {missing_key}, expected none")

// Test 6: updating existing key
user.age = 31
if user.age != 31
    errors = errors + 1
    os.output("  ERROR Test 6: user.age after update = {user.age}, expected 31")

// Test 7: adding new key
user.city = "Dubai"
if user.city != "Dubai"
    errors = errors + 1
    os.output("  ERROR Test 7: user.city = {user.city}, expected Dubai")

// Test 8: setting item to none
user.name = none
if user.name != none
    errors = errors + 1
    os.output("  ERROR Test 8: user.name after setting to none = {user.name}, expected none")

// Test 9: accessing index 0
item_zero = fruits.0
if item_zero != none
    errors = errors + 1
    os.output("  ERROR Test 9: fruits.0 = {item_zero}, expected none")

// Test 10: nested tables
company = (
    name = "Apex",
    employees = ("Alice", "Bob", "Charlie"),
    address = (
        street = "1 Sheikh Mohammed bin Rashid Boulevard",
        city = "Dubai"
    )
)
if company.name != "Apex"
    errors = errors + 1
    os.output("  ERROR Test 10: company.name = {company.name}, expected Apex")
if company.employees.1 != "Alice"
    errors = errors + 1
    os.output("  ERROR Test 10: company.employees.1 = {company.employees.1}, expected Alice")
if company.address.city != "Dubai"
    errors = errors + 1
    os.output("  ERROR Test 10: company.address.city = {company.address.city}, expected Dubai")

// Test 11: table with numbers as keys
numbers = (10, 20, 30)
if numbers.1 != 10
    errors = errors + 1
    os.output("  ERROR Test 11: numbers.1 = {numbers.1}, expected 10")
if numbers.2 != 20
    errors = errors + 1
    os.output("  ERROR Test 11: numbers.2 = {numbers.2}, expected 20")
if numbers.3 != 30
    errors = errors + 1
    os.output("  ERROR Test 11: numbers.3 = {numbers.3}, expected 30")

// Test 12: updating ordered position
fruits.2 = "blueberry"
if fruits.2 != "blueberry"
    errors = errors + 1
    os.output("  ERROR Test 12: fruits.2 after update = {fruits.2}, expected blueberry")

// Test 13: verify empty table again using different method
empty2 = ()
if empty2.1 != none or empty2.2 != none
    errors = errors + 1
    os.output("  ERROR Test 13: empty table should return none for any index")

// final result
os.output("Total errors in tables.apex: {errors}")

if errors == 0
    os.output("All tests in tables.apex passed!")
else
    os.output("{errors} test(s) failed in tables.apex.")