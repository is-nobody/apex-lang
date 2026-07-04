import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function empty_table()
    empty = []
    assert_equal(type(empty), "table", "empty table")

function ordered_list()
    colors = ["red", "green", "blue"]
    assert_equal(colors[1], "red", "colors[1]")
    assert_equal(colors[2], "green", "colors[2]")
    assert_equal(colors[3], "blue", "colors[3]")
    
    index = 2
    assert_equal(colors[index], "green", "colors[index]")

function key_value()
    user = ["name" = "Alice", "age" = 30, "active" = true]
    assert_equal(user["name"], "Alice", "user[\"name\"]")
    assert_equal(user["age"], 30, "user[\"age\"]")
    assert_equal(user["active"], true, "user[\"active\"]")

function add_keys()
    user = ["name" = "Alice"]
    assert_equal(user["name"], "Alice", "Before: user[\"name\"]")
    
    user["age"] = 30
    assert_equal(user["age"], 30, "Added age")
    
    user["city"] = "Dubai"
    assert_equal(user["city"], "Dubai", "Added city")
    
    user["active"] = true
    assert_equal(user["active"], true, "Added active")

function update_values()
    user = ["name" = "Alice", "age" = 30]
    assert_equal(user["age"], 30, "Before: user[\"age\"]")
    
    user["age"] = 31
    assert_equal(user["age"], 31, "After: user[\"age\"]")
    
    user["name"] = "Bob"
    assert_equal(user["name"], "Bob", "After: user[\"name\"]")

function mixed_tables()
    person = ["Alice", "Manager", "department" = "Engineering", "years" = 5]
    
    assert_equal(person[1], "Alice", "person[1]")
    assert_equal(person[2], "Manager", "person[2]")
    assert_equal(person["department"], "Engineering", "person[\"department\"]")
    assert_equal(person["years"], 5, "person[\"years\"]")
    
    person[3] = "Senior"
    assert_equal(person[3], "Senior", "Added position 3")

function nested_tables()
    company = [
        "name" = "Apex",
        "employees" = ["Alice", "Bob", "Charlie"],
        "address" = [
            "street" = "1 Sheikh Mohammed bin Rashid Boulevard",
            "city" = "Dubai"
        ]
    ]
    
    assert_equal(company["name"], "Apex", "company[\"name\"]")
    assert_equal(company["employees"][1], "Alice", "company[\"employees\"][1]")
    assert_equal(company["employees"][2], "Bob", "company[\"employees\"][2]")
    assert_equal(company["employees"][3], "Charlie", "company[\"employees\"][3]")
    assert_equal(company["address"]["street"], "1 Sheikh Mohammed bin Rashid Boulevard", "company[\"address\"][\"street\"]")
    assert_equal(company["address"]["city"], "Dubai", "company[\"address\"][\"city\"]")
    
    company["address"]["city"] = "Abu Dhabi"
    assert_equal(company["address"]["city"], "Abu Dhabi", "After change: company[\"address\"][\"city\"]")

function mixed_types()
    mixed = [42, "hello", true, 3.14, [-5, -10]]
    assert_equal(mixed[1], 42, "mixed[1]")
    assert_equal(mixed[2], "hello", "mixed[2]")
    assert_equal(mixed[3], true, "mixed[3]")
    assert_equal(mixed[4], 3.14, "mixed[4]")
    assert_equal(mixed[5][1], -5, "mixed[5][1]")
    assert_equal(mixed[5][2], -10, "mixed[5][2]")

function deep_nesting()
    world = [
        "continent" = [
            "country" = [
                "city" = [
                    "name" = "Dubai",
                    "population" = 3337000
                ]
            ]
        ]
    ]
    
    assert_equal(world["continent"]["country"]["city"]["name"], "Dubai", "world deep name")
    assert_equal(world["continent"]["country"]["city"]["population"], 3337000, "world deep population")

function table_as_param()
    function get_full_name(person)
        first = person["first"]
        last = person["last"]
        return "{first} {last}"
    
    user = ["first" = "John", "last" = "Doe"]
    result = get_full_name(user)
    assert_equal(result, "John Doe", "get_full_name(user)")

function function_returning_table()
    function create_point(x, y)
        return ["x" = x, "y" = y]
    
    point = create_point(10, 20)
    assert_equal(point["x"], 10, "point[\"x\"]")
    assert_equal(point["y"], 20, "point[\"y\"]")

function modify_table_in_function()
    function add_status(user, status)
        user["status"] = status
        user["modified"] = true
    
    user = ["name" = "Alice", "age" = 30]
    add_status(user, "active")
    
    assert_equal(user["name"], "Alice", "user[\"name\"]")
    assert_equal(user["age"], 30, "user[\"age\"]")
    assert_equal(user["status"], "active", "user[\"status\"]")
    assert_equal(user["modified"], true, "user[\"modified\"]")

function mixed_keys()
    data = ["first", "second", "name" = "Test", "count" = 42]
    assert_equal(data[1], "first", "data[1]")
    assert_equal(data[2], "second", "data[2]")
    assert_equal(data["name"], "Test", "data[\"name\"]")
    assert_equal(data["count"], 42, "data[\"count\"]")
    
    data[3] = "third"
    assert_equal(data[3], "third", "data[3]")

function key_existence()
    user = ["name" = "Alice", "age" = 30]
    
    has_name = user["name"] != false
    assert_equal(has_name, true, "has_name")
    
    has_city = user["city"] != false
    assert_equal(has_city, false, "has_city")

function special_values()
    special = ["empty" = "", "zero" = 0, "false_val" = false, "negative" = -1]
    assert_equal(special["empty"], "", "special[\"empty\"]")
    assert_equal(special["zero"], 0, "special[\"zero\"]")
    assert_equal(special["false_val"], false, "special[\"false_val\"]")
    assert_equal(special["negative"], -1, "special[\"negative\"]")

function manual_copy()
    original = ["name" = "Alice", "age" = 30]
    copy = ["name" = original["name"], "age" = original["age"]]
    
    copy["name"] = "Bob"
    assert_equal(original["name"], "Alice", "original[\"name\"]")
    assert_equal(copy["name"], "Bob", "copy[\"name\"]")
    assert_equal(original["age"], 30, "original[\"age\"]")
    assert_equal(copy["age"], 30, "copy[\"age\"]")

function table_as_value()
    school = [
        "name" = "Apex Academy",
        "students" = [
            ["name" = "Alice", "grade" = 95],
            ["name" = "Bob", "grade" = 87],
            ["name" = "Charlie", "grade" = 92]
        ]
    ]
    
    assert_equal(school["name"], "Apex Academy", "school[\"name\"]")
    assert_equal(school["students"][1]["name"], "Alice", "student 1 name")
    assert_equal(school["students"][1]["grade"], 95, "student 1 grade")
    assert_equal(school["students"][2]["name"], "Bob", "student 2 name")
    assert_equal(school["students"][2]["grade"], 87, "student 2 grade")
    assert_equal(school["students"][3]["name"], "Charlie", "student 3 name")
    assert_equal(school["students"][3]["grade"], 92, "student 3 grade")

function table_index_with_variable()
    t = []
    t[1] = 100
    t[2] = 200
    t[3] = 300
    
    x = 2
    v1 = t[x]
    assert_equal(v1, 200, "t[x] where x=2")
    
    sum = 0
    for i = 1, 3
        val = t[i]
        sum = sum + val
    assert_equal(sum, 600, "Sum via t[i] in loop")

function loop_variable_reuse()
    data = []
    for i = 1, 5
        data[i] = i * 10
    
    sum1 = 0
    for i = 1, 5
        sum1 = sum1 + data[i]
    assert_equal(sum1, 150, "Sum1")
    
    for i = 10, 12
        data[i] = i
    
    v10 = data[10]
    v12 = data[12]
    assert_equal(v10, 10, "data[10]")
    assert_equal(v12, 12, "data[12]")

function nested_loops()
    matrix = []
    for i = 1, 3
        matrix[i] = []
        for j = 1, 3
            matrix[i][j] = i * 10 + j
    
    sum = 0
    for i = 1, 3
        for j = 1, 3
            sum = sum + matrix[i][j]
    assert_equal(sum, 198, "Sum nested loops")

function matrix_multiplication()
    a = []
    b = []
    c = []
    
    for i = 1, 3
        a[i] = []
        b[i] = []
        c[i] = []
        for j = 1, 3
            a[i][j] = i + j
            b[i][j] = i * j
            c[i][j] = 0
    
    for i = 1, 3
        for j = 1, 3
            s = 0
            for k = 1, 3
                s = s + a[i][k] * b[k][j]
            c[i][j] = s
    
    assert_equal(c[1][1], 20, "C[1][1]")
    assert_equal(c[2][2], 52, "C[2][2]")
    assert_equal(c[3][3], 96, "C[3][3]")

function hashtable_numeric_keys()
    t = []
    for i = 1, 1000
        t[i] = i * 2
    
    sum = 0
    for i = 1, 1000
        sum = sum + t[i]
    assert_equal(sum, 1001000, "Sum 1000")

function map_filter_reduce()
    data = []
    for i = 1, 100
        data[i] = i
    
    result = 0
    for i = 1, 100
        val = data[i]
        if val % 2 == 0
            result = result + val * val
    assert_equal(result, 171700, "Result map filter reduce")

function gc_short_lived()
    sum = 0
    for i = 1, 10000
        temp = []
        for j = 1, 10
            temp[j] = i * j
        sum = sum + temp[5]
    assert_equal(sum, 250025000, "Sum gc short lived")

function table_with_none()
    t = [1, none, "hello", none, 42]
    
    assert_equal(t[2], none, "t[2] is none")
    assert_equal(t[4], none, "t[4] is none")
    assert_equal(t[2] == t[4], true, "none == none")
    assert_equal(t[2] == false, false, "none != false")
    
    t[2] = 99
    assert_equal(t[2], 99, "t[2] changed to 99")
    
    none_count = 0
    for i = 1, 5
        if t[i] == none
            none_count = none_count + 1
    assert_equal(none_count, 1, "only one none left")

empty_table()
ordered_list()
key_value()
add_keys()
update_values()
mixed_tables()
nested_tables()
mixed_types()
deep_nesting()
table_as_param()
function_returning_table()
modify_table_in_function()
mixed_keys()
key_existence()
special_values()
manual_copy()
table_as_value()
table_index_with_variable()
loop_variable_reuse()
nested_loops()
matrix_multiplication()
hashtable_numeric_keys()
map_filter_reduce()
gc_short_lived()
table_with_none()

if tests_failed == 0
    os.output("[OK] integration/table.apex")