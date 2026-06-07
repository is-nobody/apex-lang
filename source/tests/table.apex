import os

function test_empty_table()
    empty = ()
    os.output("empty = {empty}, expected false")

function test_ordered_list()
    colors = ("red", "green", "blue")
    os.output("colors.1 = {colors.1}, expected red")
    os.output("colors.2 = {colors.2}, expected green")
    os.output("colors.3 = {colors.3}, expected blue")
    
    index = 2
    os.output("colors.{index} = {colors.2}, expected green")

function test_key_value()
    user = (name = "Alice", age = 30, active = true)
    os.output("user.name = {user.name}, expected Alice")
    os.output("user.age = {user.age}, expected 30")
    os.output("user.active = {user.active}, expected true")

function test_add_keys()
    user = (name = "Alice")
    os.output("Before: user.name = {user.name}, expected Alice")
    
    user.age = 30
    os.output("Added age: user.age = {user.age}, expected 30")
    
    user.city = "Dubai"
    os.output("Added city: user.city = {user.city}, expected Dubai")
    
    user.active = true
    os.output("Added active: user.active = {user.active}, expected true")

function test_update_values()
    user = (name = "Alice", age = 30)
    os.output("Before: user.age = {user.age}, expected 30")
    
    user.age = 31
    os.output("After: user.age = {user.age}, expected 31")
    
    user.name = "Bob"
    os.output("After: user.name = {user.name}, expected Bob")

function test_mixed_tables()
    person = ("Alice", "Manager", department = "Engineering", years = 5)
    
    os.output("person.1 = {person.1}, expected Alice")
    os.output("person.2 = {person.2}, expected Manager")
    os.output("person.department = {person.department}, expected Engineering")
    os.output("person.years = {person.years}, expected 5")
    
    person.3 = "Senior"
    os.output("Added position 3: person.3 = {person.3}, expected Senior")

function test_nested_tables()
    company = (
        name = "Apex",
        employees = ("Alice", "Bob", "Charlie"),
        address = (
            street = "1 Sheikh Mohammed bin Rashid Boulevard",
            city = "Dubai"
        )
    )
    
    os.output("company.name = {company.name}, expected Apex")
    os.output("company.employees.1 = {company.employees.1}, expected Alice")
    os.output("company.employees.2 = {company.employees.2}, expected Bob")
    os.output("company.employees.3 = {company.employees.3}, expected Charlie")
    os.output("company.address.street = {company.address.street}, expected 1 Sheikh Mohammed bin Rashid Boulevard")
    os.output("company.address.city = {company.address.city}, expected Dubai")
    
    company.address.city = "Abu Dhabi"
    os.output("After change: company.address.city = {company.address.city}, expected Abu Dhabi")

function test_mixed_types()
    mixed = (42, "hello", true, 3.14, (-5, -10))
    os.output("mixed.1 = {mixed.1}, expected 42")
    os.output("mixed.2 = {mixed.2}, expected hello")
    os.output("mixed.3 = {mixed.3}, expected true")
    os.output("mixed.4 = {mixed.4}, expected 3.14")
    os.output("mixed.5.1 = {mixed.5.1}, expected -5")
    os.output("mixed.5.2 = {mixed.5.2}, expected -10")

function test_deep_nesting()
    world = (
        continent = (
            country = (
                city = (
                    name = "Dubai",
                    population = 3337000
                )
            )
        )
    )
    
    os.output("world.continent.country.city.name = {world.continent.country.city.name}, expected Dubai")
    os.output("world.continent.country.city.population = {world.continent.country.city.population}, expected 3337000")

function test_table_as_param()
    function get_full_name(person)
        return "{person.first} {person.last}"
    
    user = (first = "John", last = "Doe")
    result = get_full_name(user)
    os.output("get_full_name(user) = {result}, expected John Doe")

function test_function_returning_table()
    function create_point(x, y)
        return (x = x, y = y)
    
    point = create_point(10, 20)
    os.output("point.x = {point.x}, expected 10")
    os.output("point.y = {point.y}, expected 20")

function test_modify_table_in_function()
    function add_status(user, status)
        user.status = status
        user.modified = true
    
    user = (name = "Alice", age = 30)
    add_status(user, "active")
    
    os.output("user.name = {user.name}, expected Alice")
    os.output("user.age = {user.age}, expected 30")
    os.output("user.status = {user.status}, expected active")
    os.output("user.modified = {user.modified}, expected true")

function test_mixed_keys()
    data = ("first", "second", name = "Test", count = 42)
    os.output("data.1 = {data.1}, expected first")
    os.output("data.2 = {data.2}, expected second")
    os.output("data.name = {data.name}, expected Test")
    os.output("data.count = {data.count}, expected 42")
    
    data.3 = "third"
    os.output("data.3 = {data.3}, expected third")

function test_key_existence()
    user = (name = "Alice", age = 30)
    
    has_name = user.name != false
    os.output("has_name = {has_name}, expected true")
    
    has_city = user.city != false
    os.output("has_city = {has_city}, expected false")

function test_special_values()
    special = (empty = "", zero = 0, false_val = false, negative = -1)
    os.output("special.empty = '{special.empty}', expected ''")
    os.output("special.zero = {special.zero}, expected 0")
    os.output("special.false_val = {special.false_val}, expected false")
    os.output("special.negative = {special.negative}, expected -1")

function test_manual_copy()
    original = (name = "Alice", age = 30)
    copy = (name = original.name, age = original.age)
    
    copy.name = "Bob"
    os.output("original.name = {original.name}, expected Alice")
    os.output("copy.name = {copy.name}, expected Bob")
    os.output("original.age = {original.age}, expected 30")
    os.output("copy.age = {copy.age}, expected 30")

function test_table_as_value()
    school = (
        name = "Apex Academy",
        students = (
            (name = "Alice", grade = 95),
            (name = "Bob", grade = 87),
            (name = "Charlie", grade = 92)
        )
    )
    
    os.output("school.name = {school.name}, expected Apex Academy")
    os.output("school.students.1.name = {school.students.1.name}, expected Alice")
    os.output("school.students.1.grade = {school.students.1.grade}, expected 95")
    os.output("school.students.2.name = {school.students.2.name}, expected Bob")
    os.output("school.students.2.grade = {school.students.2.grade}, expected 87")
    os.output("school.students.3.name = {school.students.3.name}, expected Charlie")
    os.output("school.students.3.grade = {school.students.3.grade}, expected 92")

test_empty_table()
test_ordered_list()
test_key_value()
test_add_keys()
test_update_values()
test_mixed_tables()
test_nested_tables()
test_mixed_types()
test_deep_nesting()
test_table_as_param()
test_function_returning_table()
test_modify_table_in_function()
test_mixed_keys()
test_key_existence()
test_special_values()
test_manual_copy()
test_table_as_value()