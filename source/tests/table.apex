import os

function empty_table()
    empty = []
    os.output("empty = {empty}, expected false")

function ordered_list()
    colors = ["red", "green", "blue"]
    os.output("colors[1] = {colors[1]}, expected red")
    os.output("colors[2] = {colors[2]}, expected green")
    os.output("colors[3] = {colors[3]}, expected blue")
    
    index = 2
    os.output("colors[{index}] = {colors[2]}, expected green")

function key_value()
    user = ["name" = "Alice", "age" = 30, "active" = true]
    os.output("user[\"name\"] = {user[\"name\"]}, expected Alice")
    os.output("user[\"age\"] = {user[\"age\"]}, expected 30")
    os.output("user[\"active\"] = {user[\"active\"]}, expected true")

function add_keys()
    user = ["name" = "Alice"]
    os.output("Before: user[\"name\"] = {user[\"name\"]}, expected Alice")
    
    user["age"] = 30
    os.output("Added age: user[\"age\"] = {user[\"age\"]}, expected 30")
    
    user["city"] = "Dubai"
    os.output("Added city: user[\"city\"] = {user[\"city\"]}, expected Dubai")
    
    user["active"] = true
    os.output("Added active: user[\"active\"] = {user[\"active\"]}, expected true")

function update_values()
    user = ["name" = "Alice", "age" = 30]
    os.output("Before: user[\"age\"] = {user[\"age\"]}, expected 30")
    
    user["age"] = 31
    os.output("After: user[\"age\"] = {user[\"age\"]}, expected 31")
    
    user["name"] = "Bob"
    os.output("After: user[\"name\"] = {user[\"name\"]}, expected Bob")

function mixed_tables()
    person = ["Alice", "Manager", "department" = "Engineering", "years" = 5]
    
    os.output("person[1] = {person[1]}, expected Alice")
    os.output("person[2] = {person[2]}, expected Manager")
    os.output("person[\"department\"] = {person[\"department\"]}, expected Engineering")
    os.output("person[\"years\"] = {person[\"years\"]}, expected 5")
    
    person[3] = "Senior"
    os.output("Added position 3: person[3] = {person[3]}, expected Senior")

function nested_tables()
    company = [
        "name" = "Apex",
        "employees" = ["Alice", "Bob", "Charlie"],
        "address" = [
            "street" = "1 Sheikh Mohammed bin Rashid Boulevard",
            "city" = "Dubai"
        ]
    ]
    
    os.output("company[\"name\"] = {company[\"name\"]}, expected Apex")
    os.output("company[\"employees\"][1] = {company[\"employees\"][1]}, expected Alice")
    os.output("company[\"employees\"][2] = {company[\"employees\"][2]}, expected Bob")
    os.output("company[\"employees\"][3] = {company[\"employees\"][3]}, expected Charlie")
    os.output("company[\"address\"][\"street\"] = {company[\"address\"][\"street\"]}, expected 1 Sheikh Mohammed bin Rashid Boulevard")
    os.output("company[\"address\"][\"city\"] = {company[\"address\"][\"city\"]}, expected Dubai")
    
    company["address"]["city"] = "Abu Dhabi"
    os.output("After change: company[\"address\"][\"city\"] = {company[\"address\"][\"city\"]}, expected Abu Dhabi")

function mixed_types()
    mixed = [42, "hello", true, 3.14, [-5, -10]]
    os.output("mixed[1] = {mixed[1]}, expected 42")
    os.output("mixed[2] = {mixed[2]}, expected hello")
    os.output("mixed[3] = {mixed[3]}, expected true")
    os.output("mixed[4] = {mixed[4]}, expected 3.14")
    os.output("mixed[5][1] = {mixed[5][1]}, expected -5")
    os.output("mixed[5][2] = {mixed[5][2]}, expected -10")

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
    
    os.output("world[\"continent\"][\"country\"][\"city\"][\"name\"] = {world[\"continent\"][\"country\"][\"city\"][\"name\"]}, expected Dubai")
    os.output("world[\"continent\"][\"country\"][\"city\"][\"population\"] = {world[\"continent\"][\"country\"][\"city\"][\"population\"]}, expected 3337000")

function table_as_param()
    function get_full_name(person)
        return "{person[\"first\"]} {person[\"last\"]}"
    
    user = ["first" = "John", "last" = "Doe"]
    result = get_full_name(user)
    os.output("get_full_name(user) = {result}, expected John Doe")

function function_returning_table()
    function create_point(x, y)
        return ["x" = x, "y" = y]
    
    point = create_point(10, 20)
    os.output("point[\"x\"] = {point[\"x\"]}, expected 10")
    os.output("point[\"y\"] = {point[\"y\"]}, expected 20")

function modify_table_in_function()
    function add_status(user, status)
        user["status"] = status
        user["modified"] = true
    
    user = ["name" = "Alice", "age" = 30]
    add_status(user, "active")
    
    os.output("user[\"name\"] = {user[\"name\"]}, expected Alice")
    os.output("user[\"age\"] = {user[\"age\"]}, expected 30")
    os.output("user[\"status\"] = {user[\"status\"]}, expected active")
    os.output("user[\"modified\"] = {user[\"modified\"]}, expected true")

function mixed_keys()
    data = ["first", "second", "name" = "Test", "count" = 42]
    os.output("data[1] = {data[1]}, expected first")
    os.output("data[2] = {data[2]}, expected second")
    os.output("data[\"name\"] = {data[\"name\"]}, expected Test")
    os.output("data[\"count\"] = {data[\"count\"]}, expected 42")
    
    data[3] = "third"
    os.output("data[3] = {data[3]}, expected third")

function key_existence()
    user = ["name" = "Alice", "age" = 30]
    
    has_name = user["name"] != false
    os.output("has_name = {has_name}, expected true")
    
    has_city = user["city"] != false
    os.output("has_city = {has_city}, expected false")

function special_values()
    special = ["empty" = "", "zero" = 0, "false_val" = false, "negative" = -1]
    os.output("special[\"empty\"] = '{special[\"empty\"]}', expected ''")
    os.output("special[\"zero\"] = {special[\"zero\"]}, expected 0")
    os.output("special[\"false_val\"] = {special[\"false_val\"]}, expected false")
    os.output("special[\"negative\"] = {special[\"negative\"]}, expected -1")

function manual_copy()
    original = ["name" = "Alice", "age" = 30]
    copy = ["name" = original["name"], "age" = original["age"]]
    
    copy["name"] = "Bob"
    os.output("original[\"name\"] = {original[\"name\"]}, expected Alice")
    os.output("copy[\"name\"] = {copy[\"name\"]}, expected Bob")
    os.output("original[\"age\"] = {original[\"age\"]}, expected 30")
    os.output("copy[\"age\"] = {copy[\"age\"]}, expected 30")

function table_as_value()
    school = [
        "name" = "Apex Academy",
        "students" = [
            ["name" = "Alice", "grade" = 95],
            ["name" = "Bob", "grade" = 87],
            ["name" = "Charlie", "grade" = 92]
        ]
    ]
    
    os.output("school[\"name\"] = {school[\"name\"]}, expected Apex Academy")
    os.output("school[\"students\"][1][\"name\"] = {school[\"students\"][1][\"name\"]}, expected Alice")
    os.output("school[\"students\"][1][\"grade\"] = {school[\"students\"][1][\"grade\"]}, expected 95")
    os.output("school[\"students\"][2][\"name\"] = {school[\"students\"][2][\"name\"]}, expected Bob")
    os.output("school[\"students\"][2][\"grade\"] = {school[\"students\"][2][\"grade\"]}, expected 87")
    os.output("school[\"students\"][3][\"name\"] = {school[\"students\"][3][\"name\"]}, expected Charlie")
    os.output("school[\"students\"][3][\"grade\"] = {school[\"students\"][3][\"grade\"]}, expected 92")

function table_index_with_variable()
    t = []
    t[1] = 100
    t[2] = 200
    t[3] = 300
    
    x = 2
    v1 = t[x]
    os.output("t[x] where x=2: {v1}, expected 200")
    
    sum = 0
    for i = 1, 3
        val = t[i]
        sum = sum + val
    os.output("Sum via t[i] in loop: {sum}, expected 600")

function loop_variable_reuse()
    data = []
    for i = 1, 5
        data[i] = i * 10
    
    sum1 = 0
    for i = 1, 5
        sum1 = sum1 + data[i]
    os.output("Sum1: {sum1}, expected 150")
    
    for i = 10, 12
        data[i] = i
    
    v10 = data[10]
    v12 = data[12]
    os.output("data[10]={v10}, data[12]={v12}")

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
    os.output("Sum: {sum}, expected 198")

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
    
    v11 = c[1][1]
    v22 = c[2][2]
    v33 = c[3][3]
    os.output("C[1][1]={v11}, expected 20")
    os.output("C[2][2]={v22}, expected 52")
    os.output("C[3][3]={v33}, expected 96")

function hashtable_numeric_keys()
    t = []
    for i = 1, 1000
        t[i] = i * 2
    
    sum = 0
    for i = 1, 1000
        sum = sum + t[i]
    os.output("Sum: {sum}, expected 1001000")

function map_filter_reduce()
    data = []
    for i = 1, 100
        data[i] = i
    
    result = 0
    for i = 1, 100
        val = data[i]
        if val % 2 == 0
            result = result + val * val
    os.output("Result: {result}, expected 171700")

function gc_short_lived()
    sum = 0
    for i = 1, 10000
        temp = []
        for j = 1, 10
            temp[j] = i * j
        sum = sum + temp[5]
    os.output("Sum: {sum}, expected 250025000")

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