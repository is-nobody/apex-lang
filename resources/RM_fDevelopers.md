# Apex Language Reference Manual for Beginners
This manual is minimalistic. Each section builds on the previous ones. For the best experience, follow the order.

## Table of Contents
### Introduction
- [About the Project](#about-the-project)
- [Setting up workspace](#setting-up-workspace)

### 1. Variables & Data Types
- [1.1 None](#11-none)
- [1.2 Numbers](#12-numbers)
- [1.3 Booleans](#13-booleans)
- [1.4 Strings](#14-strings)
- [1.5 Tables](#15-tables)

### 2. Operators
- [2.1 Arithmetic Operators](#21-arithmetic-operators)
- [2.2 Comparison Operators](#22-comparison-operators)
- [2.3 Logical Operators](#23-logical-operators)

### 3. If Statements
- [3.1 If Statement](#31-if-statement)
- [3.2 Elif Statement](#32-elif-statement)
- [3.3 Else Statement](#33-else-statement)

### 4. While Loops
- [4.1 While Statement](#41-while-statement)
- [4.2 Break](#42-break)
- [4.3 Continue](#43-continue)

### 5. For Loops
- [5.1 For Statement](#51-for-statement)
- [5.2 Range](#52-range)
- [5.3 Break](#53-break)
- [5.4 Continue](#54-continue)

### 6. Error Handling
- [6.1 Try Statement](#61-try-statement)
- [6.2 Failure Statement](#62-failure-statement)
- [6.3 Always Statement](#63-always-statement)

### 7. Functions
- [7.1 Function Statement](#71-function-statement)
- [7.2 Parameters](#72-parameters)
- [7.3 Return Value](#73-return-value)
- [7.4 Call](#74-call)

### 8. Imports
- [8.1 Importing an Entire File](#81-importing-an-entire-file)
- [8.2 Importing from Sub-folders](#82-importing-from-sub-folders)
- [8.3 Importing from One Sub-folder into Another](#83-importing-from-one-sub-folder-into-another)

### 9. Built-In Libraries
- [9.1 OS Library (os)](#91-os-library-os)
- [9.2 Math Library (math)](#92-math-library-math)
- [9.3 String Library (string)](#93-string-library-string)
- [9.4 Network Library (network)](#94-network-library-network)

### Conclusion
- [What's Next?](#whats-next)

# Introduction
## About the Project
Apex is a programming language designed for simplicity and cross-platform development. Apex is under the MIT License. All created by one person.

## Setting up workspace
### Installing
1. Download the interpreter from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the interpreter for your OS.

Now you're in REPL!

### Testing the interpreter
Create your own file with `main.apex` name:

```apex
// main.apex
import os
os.output("Hello, Friend")
```

Execute it via REPL in current directory:

```bash
main.apex
```

# 1. Data Types
Apex determines the type automatically. Main data types in language:

| Type | Description | Example |
|------|-------------|---------|
| `None` | Nothing, no value | `x = none` |
| `Number` | Numbers (Wholes and decimals) | `x = 10`, `x = 3.14` |
| `String` | Text, sequence of characters | `x = "hello"` |
| `Boolean` | True or false | `x = true`, `x = false` |
| `Table` | Universal container | `x = (1, 2, 3)`, `x = (name = "John")` |

For `none`, `true`, and `false`, case doesn't matter.

`//` means a comment.

## 1.1 None
None represents nothing — no value, empty, undefined.

```apex
x = none        // x exists but has no value
y = 10          // y had a value
y = none        // now y empty
```

## 1.2 Numbers
In Apex, you don't need to worry about whether a number is a whole number or a decimal. Apex figures out the rest. There is no limit to numbers.

### Whole Numbers
Whole numbers are written without any other symbols. They can be positive or negative.

```apex
year = 2024
temperature = -5
```

### Decimal Numbers
Decimals are written with a dot `.`.

```apex
weight = 71.5
height = 1.8
```

### Declaring with Arithmetic
You can declare variables and do math with them at the same time. Once a variable holds a number, you can use it in calculations just like a regular number.

```apex
x = 10
y = 3.5
sum = x + y
```

## 1.3 Booleans
Booleans represent one of two possible values: `true` or `false`. You can create booleans in two ways: directly or through comparisons.

Direct assignment:

```apex
is_active = true
has_permission = false
is_logged_in = true
```

Through comparisons:

```apex
age = 25
is_adult = age > 18        // true — because 25 is greater than 18
```

Whenever you use comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`), the result is always a boolean. Booleans also come from logical operations that combine values.

## 1.4 Strings
A string can be one character long, a thousand characters long, or even empty.

### Creating Strings
In Apex, strings are written inside double quotes `" "`:

```apex
name = "Alice"
message = "Hello, Friend"
empty = ""
```

If you need double quotes inside them, just do it, Apex will take them calmly, since they are already inside the main double quotes:

```apex
inside = "He say: "I hate donuts!""
```

### String Interpolation
Apex have only one way for gluing a string with a variable — string interpolation.

```apex
score = 95
price = 19.99
is_active = true

result = "Score: {score}, Price: {price}, Active: {is_active}"
```

Apex automatically converts numbers and other values to text when you put them inside `{}`. Inside them, you can only specify the name of the variable, but not the expression.

## 1.5 Tables
A table is Apex's universal container. Tables are flexible — they work as ordered lists and key-value pairs. You can even mix both styles in the same table.

### Creating Tables
Tables are written inside parentheses `( )`. Values are separated by commas.

```apex
empty = ()                               // empty table — nothing inside
fruits = ("apple", "banana", "orange")   // three items
numbers = (10, 20, 30, 40)               // four numbers
mixed = (42, "hello", true)              // different types together
```

### Ordered Lists
When you list values without keys, you create an ordered list. Each value has a position — starting from 1.

```apex
colors = ("red", "green", "blue")
// Access by position
first_color = colors.1      // "red"
second_color = colors.2     // "green"
third_color = colors.3      // "blue"
```

### Key-Value Pairs
When you want to label each value with a name, use keys. Keys and values are connected with `=`. You can't use numbers as keys, because numbers are reserved and using for calling items without keys.

```apex
user = (
    name = "Alice",
    age = 30,
    active = true
)
```

Keys are written without quotes. Apex recognizes them as names, not strings. Now you can access values by their key:

```apex
user_name = user.name         // "Alice"
user_age = user.age           // 30
user_active = user.active     // true
```

### Working with Keys
Once a table exists, you can working with keys:

```apex
user = (name = "Alice")
// Add a new key
user.age = 30
user.city = "Dubai"
user.active = true
// Now user has four keys
// (name = "Alice", age = 30, city = "Dubai", active = true)
```

Updating a value works the same way — just assign a new value to an existing key or position. If you access a non-existent key, you will get none. If you want to remove an item from a table, you need to assign it the value `none`. If you need the `0` item, you will get a `none` value.

### Mixed Tables
Tables can combine ordered items and key-value pairs in the same table. Ordered items come first, then key-value pairs:

```apex
person = ("Alice", "Manager", department = "Engineering", years = 5)
// Access ordered items by position
name = person.1
role = person.2
// Access key-value pairs by key
dept = person.department
experience = person.years
```

### Tables Inside Tables
Tables can contain other tables. This lets you build complex data structures:

```apex
company = (
    name = "Apex",
    employees = ("Alice", "Bob", "Charlie"),
    address = (
        street = "1 Sheikh Mohammed bin Rashid Boulevard",
        city = "Dubai"
    )
)
// Access nested values
company_name = company.name
first_employee = company.employees.1
city = company.address.city
```

# 2. Operators
## 2.1 Arithmetic Operators
Arithmetic operators work with numbers. They do exactly what you learned in math class.

| Operator | Name | Example | Result |
|----------|------|---------|--------|
| `+` | Addition | `5 + 3` | `8` |
| `-` | Subtraction | `10 - 4` | `6` |
| `*` | Multiplication | `7 * 6` | `42` |
| `/` | Division | `15 / 4` | `3.75` |
| `%` | Modulo (remainder) | `15 % 4` | `3` |

Arithmetic only works with the numbers data type, you cannot add number with string, none with boolean, etc. When you perform an arithmetic operation between a whole and a decimal, the result also becomes a decimal.

## 2.2 Comparison Operators
Comparison operators compare two values and give you a boolean result: either `true` or `false`. They can return only a Boolean value.

| Operator | Name | Example | Result |
|----------|------|---------|--------|
| `==` | Equal to | `5 == 5` | `true` |
| `!=` | Not equal to | `5 != 3` | `true` |
| `<` | Less than | `3 < 5` | `true` |
| `>` | Greater than | `5 > 3` | `true` |
| `<=` | Less than or equal | `3 <= 3` | `true` |
| `>=` | Greater than or equal | `5 >= 5` | `true` |

Comparison results are booleans — you can store them in variables.

### String Comparisons
You can compare strings, but only for similarity, using `==` and `!=`:

```apex
"Bob" == "Bob"     // true
"Bob" != "Bob"     // false
```

## 2.3 Logical Operators
Logical operators combine boolean values (`true` or `false`) to create more complex conditions.

| Operator | Name | What It Does | Example | Result |
|----------|------|--------------|---------|--------|
| `and` | AND | Both sides must be true | `true and true` | `true` |
| `or` | OR | At least one side must be true | `true or false` | `true` |
| `not` | NOT | Reverses the value | `not true` | `false` |

### Operator Precedence
Logical operators have their own order. `not` happens first, then `and`, then `or`.

Full precedence order (highest to lowest):

1. `()` — parentheses
2. `*`, `/`, `%` — multiplication, division, modulo
3. `+`, `-` — addition, subtraction
4. `<`, `>`, `<=`, `>=` — comparisons
5. `==`, `!=` — equality
6. `not` — logical NOT
7. `and` — logical AND
8. `or` — logical OR

# 3. If Statements
If statements are how you tell Apex to make decisions.

| Statement | When It Runs |
|-----------|---------------|
| `if` | Condition is `true` |
| `elif` | Previous conditions were `false` AND this condition is `true` |
| `else` | All previous conditions were `false` |

Apex does not treat `none` as `false`. Always compare with `== none` or `!= none`.

## 3.1 If Statement
If user doesn't exist, then display output about him. But if we have user, we does nothing and moves on.

```apex
import os
user = none
if user == none
    os.output("No user found")
```

## 3.2 Elif Statement
Sometimes one condition isn't enough. Use `elif` — short for "else if".

```apex
import os
score = 85
if score >= 90
    os.output("Grade: A")
elif score >= 80
    os.output("Grade: B")
```

Apex checks conditions in order from top to bottom. As soon as one condition is `true`, it runs that block and skips the rest. The remaining elif blocks are never checked.

## 3.3 Else Statement
`else` catches everything that wasn't caught by `if` or `elif`. It runs when no other condition was `true`.

```apex
import os
score = 55
if score >= 60
    os.output("You passed!")
else
    os.output("You failed.")
```

# 4. While Loops
## 4.1 While Statement
As long as condition is `true`, Apex keeps running the code inside. When condition becomes `false`, Apex exits the loop and continues with the rest of the program. Simple Example:

Count from 1 to 5:

```apex
import os
counter = 1
while counter <= 5
    os.output(counter)
    counter = counter + 1
```

### 4.2 Break
`break` immediately exits the loop, no matter what the condition says. Use it when you need to stop early.

```apex
import os
x = 1
while x <= 10
    if x == 5
        break
    os.output(x)
    x = x + 1
```

Nothing after `break` runs for that iteration.

### 4.3 Continue
`continue` skips the rest of the current iteration and jumps to the next check of the condition.

```apex
import os
x = 0
while x < 10
    x = x + 1
    if x % 2 == 0
        continue
    os.output(x)
```

When `x` is even, `continue` skips `os.output(x)` and goes back to check the condition again.

# 5. For Loops
## 5.1 For Statement
A `for` loop repeats code once for each item in a collection. You give it a variable and a table. The loop runs once per item, and each time the variable holds the next value.

## 5.2 Range
The `range()` function creates a table with numbers.

```apex
import os
for i in range(1, 6)
    os.output(i)
```

`range(1, 6)` creates the table `(1, 2, 3, 4, 5)`.

### Range with Step
Add a third parameter to skip numbers — the step size.

```apex
import os
for i in range(0, 11, 2)
    os.output(i)
```

This prints even numbers: 0, 2, 4, 6, 8, 10. `range(0, 11, 2)` creates `(0, 2, 4, 6, 8, 10)`.

### Counting Down
Use a negative step to count backward.

```apex
import os
for i in range(5, 0, -1)
    os.output(i)
```

This prints 5, 4, 3, 2, 1.

## 5.3 Break
`break` exits the loop immediately — same as in `while`.

```apex
import os
for i in range(1, 11)
    if i == 5
        break
    os.output(i)
```

## 5.4 Continue
`continue` skips the rest of the current iteration and moves to the next item.

```apex
import os
for i in range(1, 6)
    if i == 3
        continue
    os.output(i)
```

# 6. Error Handling
Error handling allows your program to attempt risky operations and recover gracefully when they fail, instead of crashing.

## 6.1 Try Statement
Use `try` to wrap code that might fail. If something goes wrong, the program jumps to the `failure` block instead of crashing.

```apex
import os
try
    file_content = os.read("data.txt")
    os.output("File loaded successfully")
failure
    os.output("Could not read file — using default values")
    file_content = none
```

## 6.2 Failure Statement
The `failure` block only runs when an error occurs. It's your safety net.

```apex
import os

file_content = none

try
    file_content = os.read("data.txt")
    os.output("File loaded: {file_content}")
failure
    os.output("Could not read file — using default value")
    file_content = "default content"

// Program continues either way
os.output("Final content: {file_content}")
```

## 6.3 Always Statement
Sometimes you need code that runs whether an error happened or not.

```apex
import os

file_content = none

try
    file_content = os.read("data.txt")
    os.output("File loaded successfully")
failure
    os.output("Could not read file — using default value instead")
    file_content = "default content"
always
    // This runs no matter what — success or failure
    os.output("Done. Content ready: {file_content}")
```

# 7. Functions
## 7.1 Function Statement
To create a function, use the `function` keyword, then the function name, then parentheses `( )`, then the code block.

```apex
import os

function say_hello()
    os.output("Hello!")
```

## 7.2 Parameters
Sometimes a function needs information to do its job. You put them inside the parentheses.

```apex
import os

function greet(name)
    os.output("Hello, {name}!")
```

You can have multiple parameters, separated by commas. Order matters. The first value goes to the first parameter, the second value to the second parameter, and so on.

## 7.3 Return Value
Return value is what the function sends back after it finishes. You use the `return` keyword.

```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)    // Prints: 8
```

Functions without `return` return `none` automatically. Once `return` happens, the function exits. Nothing after it runs.
### 7.4 Call
Using a function is called calling it. You write the function name followed by parentheses.
```apex
// Call a function with no parameters
say_hello()
// Call with parameters
greet("Alice")
// Store the return value
total = add(10, 5)
```

Direct call:

```apex
import os

function show_message()
    os.output("Function called!")
show_message()    // Prints: Function called!
```

Call in expressions:

```apex
function triple(x)
    return x * 3
result = triple(4) + 10    // 12 + 10 = 22
```

Nested calls (call a function inside another call):

```apex
function add(a, b)
    return a + b
function multiply(a, b)
    return a * b
// Nested — multiply happens first, then add
result = add(5, multiply(2, 3))        // add(5, 6) = 11
```

Calling a function that calls another function:

```apex
function get_discount(price)
    return price * 0.9
function calculate_total(price, quantity)
    total = price * quantity
    return get_discount(total)   // Calls another function
final_price = calculate_total(100, 3)   // 100 * 3 = 300, then 300 * 0.9 = 270
```

# 8. Imports
Imports give you the ability to use code from other files. Every import path is relative to the main file — the file you run with `apex filename.apex`.

## 8.1 Importing an Entire File
To import everything from a file in the same folder:

```apex
import os
import database

// Use items with the filename as a prefix
database.connect()
os.output(database.APP_NAME)
```

When you import a file, you must use the filename as a prefix to access its contents.

## 8.2 Importing from Sub-folders
Use dots (`.`) to navigate into folders:

```
my_project/
├── main.apex
├── utils/
│   ├── math.apex
└── └── string.apex
```

```apex
import utils.math
```

Each dot in imports means "go inside this folder." `utils.math` looks for `utils/math.apex`.

## 8.3 Importing from One Sub-folder into Another
You have this structure:

```
my_project/
├── main.apex
├── helpers/
│   └── math.apex
└── features/
    └── calculator.apex
```

You want to use `math.apex` inside `calculator.apex`. Always write the path as if you were importing from `main.apex`. Apex always starts looking from the main file's folder. This keeps your imports consistent — no matter how deep your folder structure gets, you always know exactly how to import any file.

### What Gets Imported
When you import a file, you get everything from it:

- All functions
- All variables

# 9. Built-in Libraries
What is this?

## 9.1 OS Library (os)
Functions for interacting with the operating system.

### os.output()
Prints text to the terminal.
```apex
import os
os.output("Hello")
```
### os.input()
Reads a line from the user.
```apex
import os
name = os.input()
os.output("Hello, {name}")
```

## 9.2 Math Library (math)
math: abs, floor, ceil, round, sqrt, sin, cos, tan, pi, random.

## 9.3 String Library (string)
string: len, lower, upper, sub, split, join, trim, find, replace.

## 9.4 Network Library (network)
url_encode(str)
url_decode(str)
parse_url(url)

http_get(url, headers)
http_post(url, body, headers)
http_set_timeout(seconds)

tcp_connect(host, port)
socket.send(data)
socket.receive(bytes)
socket.close()
socket.set_timeout(seconds)

tcp_listen(port)
server.accept()
server.close()

websocket_connect(url)
ws.send(data)
ws.on_message

dns_lookup(hostname)

ip_is_valid(ip)