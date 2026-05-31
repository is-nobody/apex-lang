# Apex Language Reference Manual for Beginners
This manual is minimalistic. Each section builds on the previous ones. For the best experience, follow the order.

## Table of Contents
### Introduction
- [About the Project](#about-the-project)
- [Setting up workspace](#setting-up-workspace)

### 1. Variables & Data Types
- [1.1 Numbers](#11-numbers)
- [1.2 Booleans](#12-booleans)
- [1.3 Strings](#13-strings)
- [1.4 Tables](#14-tables)
- [1.5 Type Conversion](#15-table-conversion)

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

### 6. Functions
- [6.1 Function Statement](#61-function-statement)
- [6.2 Parameters](#62-parameters)
- [6.3 Return Value](#63-return-value)
- [6.4 Call](#64-call)

### 7. Imports
- [7.1 Importing an Entire File](#71-importing-an-entire-file)
- [7.2 Importing from Sub-folders](#72-importing-from-sub-folders)
- [7.3 Importing from One Sub-folder into Another](#73-importing-from-one-sub-folder-into-another)

### 8. Built-In Libraries
- [8.1 OS Library (os)](#81-os-library-os)
- [8.2 Math Library (math)](#82-math-library-math)
- [8.3 String Library (string)](#83-string-library-string)
- [8.4 Table Library (table)](#84-table-library-table)

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
Paste this code into REPL:

```apex
import os
os.output("Hello, Friend")
```

Output:

```bash
Hello, Friend
```

# 1. Data Types
Apex determines the type automatically. Main data types in language:

| Type | Description | Example |
|------|-------------|---------|
| `number` | Numbers (Wholes and decimals) | `x = 10`, `x = 3.14` |
| `string` | Text, sequence of characters | `x = "hello"` |
| `boolean` | True or false | `x = true`, `x = false` |
| `table` | Universal container | `x = (1, 2, 3)`, `x = (name = "John")` |

The type of a variable cannot change. If a variable was created as a `string`, it can never become a `number`.

For `true` and `false` use lowercase.

`//` means a comment.

## 1.1 Numbers
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

## 1.2 Booleans
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

## 1.3 Strings
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

Apex automatically converts numbers and other values to strings when you put them inside `{}`. 

Inside the braces, you can also use numeric expressions — they are evaluated first, and then the result is converted to a string:

```apex
import os
count = 5
os.output("Total: {count * 2}")
```

## 1.4 Tables
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

Updating a value works the same way — just assign a new value to an existing key or position. If you access a non-existent key, you will get an error. If you need the `0` item, you will get an error. To remove an item from a table, use the `table.remove()` function from the table library *(see section 9.4)*.

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

## 1.5 Type Conversion
Apex provides two built-in functions for explicit type conversion:

| Function | What it does | Example |
|----------|--------------|---------|
| `number(x)` | Converts to number | `number("42")` → `42` |
| `string(x)` | Converts to string | `string(42)` → `"42"` |

### number()
Converts a value to number. You will get an error if conversion fails.

```apex
number("42")       // 42
number("3.14")     // 3.14
number(true)       // error (conversion fails)
number(false)      // error (conversion fails)
number("hello")    // error (conversion fails)
```

### string(x)
Converts any value to its string representation.

| Input | Output |
|-------|--------|
| `42` | `"42"` |
| `3.14` | `"3.14"` |
| `true` / `false` | `"true"` / `"false"` |

```apex
string(42)      // "42"
string(3.14)    // "3.14"
string(true)    // "true"
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

Arithmetic only works with the numbers data type, you cannot add number with string, string with boolean, etc. When you perform an arithmetic operation between a whole and a decimal, the result also becomes a decimal.

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

## 3.1 If Statement
If user doesn't exist, then display output about him. But if we have user, we does nothing and moves on.

```apex
import os
user = false
if user == false
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

## 4.2 Break
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

## 4.3 Continue
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

# 6. Functions
## 6.1 Function Statement
To create a function, use the `function` keyword, then the function name, then parentheses `( )`, then the code block.

```apex
import os

function say_hello()
    os.output("Hello!")
```

## 6.2 Parameters
Sometimes a function needs information to do its job. You put them inside the parentheses.

```apex
import os

function greet(name)
    os.output("Hello, {name}!")

greet("Friend")
```

You can have multiple parameters, separated by commas. Order matters. The first value goes to the first parameter, the second value to the second parameter, and so on.

Sometimes you need a function to accept a specific data type of a variable. For this, you need to use `==` and specify the type:

```apex
import os

function add(a == number, b == number)
    os.output(a + b)
add(5, 6)
```

## 6.3 Return Value
Return value is what the function sends back after it finishes. You use the `return` keyword.

```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)    // Prints: 8
```

Functions without `return` return `false` automatically. Once `return` happens, the function exits. Nothing after it runs.

### 6.4 Call
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

# 7. Imports
Imports give you the ability to use code from other files. Every import path is relative to the main file — the file you run with `apex filename.apex`.

## 7.1 Importing an Entire File
To import everything from a file in the same folder:

```apex
import os
import database

// Use items with the filename as a prefix
database.connect()
os.output(database.APP_NAME)
```

When you import a file, you must use the filename as a prefix to access its contents.

## 7.2 Importing from Sub-folders
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

## 7.3 Importing from One Sub-folder into Another
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

# 8. Built-in Libraries
## 8.1 OS Library (os)
The OS library lets you interact with the operating system. You can print text, read files, create folders, and more. Import it with `import os`.

### os.output(value)
Prints a value to the terminal.

```apex
import os
os.output("Hello, Friend")     // Hello, Friend
os.output(42)                  // 42
os.output(true)                // true
```

### os.input(prompt)
Waits for the user to type something and press Enter. Returns what they typed as a string. You can provide an optional prompt message.

```apex
import os
name = os.input("What is your name? ")
os.output("Hello, {name}")
```

If the user types `Alice`, the output is `Hello, Alice`. The value from `os.input()` is always a string — even if the user types numbers. Use `number()` to convert it if you need to do math. If the user enters something that isn't a number, `number()` will get an `false` value. Handle it via `if var == false`.

### os.read(filename)
Reads the entire contents of a file and returns it as a string. Throws an error if the file doesn't exist or can't be read.

```apex
import os

content = os.read("story.txt")

if content == false
    os.output("Could not read the file")
```

### os.write(filename, content)
Writes content to a file. Creates the file if it doesn't exist, overwrites it if it does. Returns `true` on success, `false` on failure.

```apex
import os

success = os.write("notes.txt", "Today I learned Apex!")

if success == true
    os.output("File saved!")
```

### os.close(filename)
Closes an open file. If no filename is given, closes all open files. You normally don't need to call this — files close automatically. Use it only if you're working with many files and want to free resources.

```apex
os.close("data.txt")    // Close specific file
os.close()              // Close all open files
```

### os.exists(path)
Returns `true` if the file or folder at `path` exists, `false` otherwise.

```apex
import os

if os.exists("config.apex") == true
    os.output("Config file found")
else
    os.output("No config file")
```

### os.isfile(path)
Returns `true` if the path points to a file specifically (not a folder).

```apex
os.isfile("data.txt")       // true if it's a file
os.isfile("my_folder")      // false — it's a folder, not a file
```

### os.isdir(path)
Returns `true` if the path points to a folder specifically.

```apex
os.isdir("my_folder")       // true if it's a folder
os.isdir("data.txt")        // false — it's a file, not a folder
```

### os.rename(old_name, new_name)
Renames a file or folder. Returns `true` on success, `false` on failure.

```apex
os.rename("old_name.txt", "new_name.txt")
```

### os.rmfile(path)
Deletes a file permanently. Returns `true` if deleted, `false` if the file doesn't exist or can't be deleted.

```apex
import os
if os.rmfile("temp.txt")
    os.output("File deleted")
else
    os.output("Could not delete file")
```

### os.mkfile(filename)
Creates an empty file. Returns `true` on success, `false` on failure.

```apex
os.mkfile("new_file.txt")   // Creates empty file
```

### os.listdir(path)
Returns a table of names — all files and folders inside the given folder. If no path is given, lists the current folder. Returns an empty table on failure.

```apex
import os
items = os.listdir(".")           // List current folder
for item in items
    os.output(item)
```

### os.getcwd()
Returns the current working directory as a string — where your program is running from.

```apex
import os
current_folder = os.getcwd()
os.output("Running from: {current_folder}")
```

### os.chdir(path)
Changes the current working directory. Returns `true` on success, `false` on failure.

```apex
os.chdir("/home/user/projects")   // Move to another folder
os.chdir("..")                    // Go up one folder
```

### os.mkdir(path)
Creates a new folder. Returns `true` on success, `false` if the folder already exists or can't be created.

```apex
import os
if os.mkdir("my_project")
    os.output("Folder created")
else
    os.output("Folder already exists or can't be created")
```

### os.rmdir(path)
Removes an empty folder. Returns `true` on success, `false` if the folder isn't empty or doesn't exist. This only works on empty folders — use `os.rmfile()` to delete files inside first.

```apex
os.rmdir("old_folder")
```

### os.stat(path)
Returns a table with information about a file or folder. The table contains these keys:
- `size` — file size in bytes
- `mtime` — last modification time (timestamp)
- `ctime` — creation time (timestamp)
- `isdir` — `true` if it's a folder, `false` if it's a file

Throws an `false` value if the path doesn't exist.

```apex
import os

info = os.stat("data.txt")

if info == true
    os.output("Size: {info.size} bytes")
failure
    os.output("File does not exist")
```

### os.exit(code)
Exits the program immediately. The `code` is optional — `0` means success, other numbers mean an error. If no code is given, uses `0`.

```apex
import os

if os.exists("critical_file.txt") == false
    os.output("Critical file missing — exiting")
    os.exit(1)              // Exit with error code
```

### os.wait(seconds)
Pauses the program for the given number of seconds. You can use decimals for fractions of a second. Negative values are treated as `0`.

```apex
import os

os.output("Starting...")
os.wait(2.5)                // Pause for 2.5 seconds
os.output("Done waiting")
```

### os.time()
Returns the current time as a number — seconds since January 1, 1970. Useful for measuring how long something takes.

```apex
import os

start = os.time()
// ... do something slow ...
elapsed = os.time() - start
os.output("Took {elapsed} seconds")
```

### os.system(command)
Runs a system command as if you typed it in the terminal. Returns the command's exit code. Available commands depend on your operating system.

```apex
import os

os.system("echo Hello from terminal")     // Works on Linux/macOS
os.system("dir")                          // Windows — list files
os.system("ls")                           // Linux/macOS — list files
```

### os.platform()
Returns a string identifying your operating system: `"Windows"`, `"macOS"`, `"Linux"`, or `"Unknown OS"`.

```apex
import os

system = os.platform()
os.output("You're running on {system}")
```

## 8.2 Math Library (math)
The Math library provides mathematical functions beyond basic arithmetic. Import it with `import math`.

### math.abs(x)
Returns the absolute value of a number — how far it is from zero, ignoring the sign.

```apex
import math
math.abs(5)        // 5
math.abs(-5)       // 5
math.abs(-3.14)    // 3.14
```

### math.floor(x)
Rounds a number down to the nearest whole number. Think of it as "cut off the decimal part, go lower."

```apex
import math
math.floor(3.7)     // 3
math.floor(3.1)     // 3
math.floor(-2.3)    // -3 (goes down, so more negative)
```

### math.ceil(x)
Rounds a number up to the nearest whole number. Think of it as "push up to the next integer."

```apex
import math
math.ceil(3.1)      // 4
math.ceil(3.7)      // 4
math.ceil(-2.3)     // -2 (goes up toward zero)
```

### math.round(x, digits)
Rounds a number to the nearest whole number, or to a specific number of decimal places. The `digits` parameter is optional — without it, rounds to a whole number.

```apex
import math
math.round(3.4)         // 3
math.round(3.6)         // 4
math.round(3.5)         // 4 (rounds up at .5)
math.round(3.14159, 2)  // 3.14
math.round(3.14159, 3)  // 3.142
```

### math.sqrt(x)
Returns the square root of a number. Throws an error for negative numbers — you can't take the square root of a negative in real numbers.

```apex
import math
math.sqrt(25)      // 5
math.sqrt(2)       // 1.4142135623730951
math.sqrt(-1)      // error
```

### math.exp(x)
Returns `e` raised to the power of `x`. `e` is Euler's number (approximately 2.71828), used in growth rates, compound interest, and natural processes.

```apex
import math
math.exp(1)        // 2.718281828459045
math.exp(0)        // 1
math.exp(2)        // 7.38905609893065
```

### math.log(x, base)
Returns the logarithm of `x`. Without a base, uses the natural logarithm (base `e`). With a base, calculates the logarithm with that base. Throws an error if `x` is zero or negative — logarithms aren't defined for those values.

```apex
import math
math.log(2.71828)       // ~1 (natural log of e)
math.log(100, 10)       // 2 (10² = 100)
math.log(8, 2)          // 3 (2³ = 8)
math.log(0)        // error
math.log(-5)       // error
```

### Trigonometry
All trigonometric functions work with **radians**, not degrees. Radians are another way to measure angles — a full circle is 2π radians, which equals 360 degrees.

If you have degrees and need radians: multiply by `math.pi` and divide by 180. Apex doesn't have a built-in `math.pi` constant, but you can create one:

```apex
import math
pi = math.asin(1) * 2    // 3.141592653589793
degrees = 90
radians = degrees * pi / 180
```

### math.sin(x)
Returns the sine of `x` radians.

```apex
import math
math.sin(0)              // 0
math.sin(math.asin(1))   // ~1 (sine of π/2 is 1)
```

### math.cos(x)
Returns the cosine of `x` radians.

```apex
import math
math.cos(0)              // 1
math.cos(math.asin(1))   // ~0 (cosine of π/2 is 0)
```

### math.tan(x)
Returns the tangent of `x` radians.

```apex
import math
math.tan(0)              // 0
math.tan(math.asin(1)/4) // ~1 (tangent of π/4 is 1)
```

### math.asin(x)
Returns the arcsine of `x` in radians — the angle whose sine is `x`. Input must be between -1 and 1. Throws an error if outside that range.

```apex
import math
math.asin(0)             // 0
math.asin(1)             // 1.5707963267948966 (π/2)
math.asin(2)             // error
```

### math.acos(x)
Returns the arccosine of `x` in radians — the angle whose cosine is `x`. Input must be between -1 and 1. Throws an error if outside that range.

```apex
import math
math.acos(1)             // 0
math.acos(0)             // 1.5707963267948966 (π/2)
math.acos(2)             // error
```

### math.atan(x)
Returns the arctangent of `x` in radians — the angle whose tangent is `x`.

```apex
import math
math.atan(0)             // 0
math.atan(1)             // 0.7853981633974483 (π/4)
```

## 8.3 String Library (string)
The String library helps you work with text — measure length, change case, find words, split and combine strings, and more. Import it with `import string`.

### string.len(s)
Returns the number of characters in a string. Spaces and punctuation count as characters too.

```apex
import string
string.len("hello")        // 5
string.len("")             // 0
string.len("hi there")     // 8 (space counts)
string.len("Apex!")        // 5
```

### string.lower(s)
Converts every character in the string to lowercase. Useful when you want to compare text without worrying about capitalization.

```apex
import string
string.lower("HELLO")      // "hello"
string.lower("Hello")      // "hello"
string.lower("Apex 123")   // "apex 123"
```

### string.upper(s)
Converts every character in the string to uppercase.

```apex
import string
string.upper("hello")      // "HELLO"
string.upper("Hello")      // "HELLO"
string.upper("apex 123")   // "APEX 123"
```

### string.sub(s, start, end)
Extracts a portion of a string — from `start` to `end`, but not including `end`. Think of it as "give me characters from position start up to just before end."

Positions start counting from `0`, not `1`. The first character is at position 0.

```apex
import string
text = "Hello, World"
string.sub(text, 0, 5)     // "Hello"
string.sub(text, 7, 12)    // "World"
string.sub(text, 0, 1)     // "H" (first character only)
```

If `start` is negative, it's treated as `0`. If `end` is larger than the string length, it stops at the end.

```apex
string.sub("Apex", 1, 10)  // "pex" (end is bigger than string, stops at end)
```

### string.split(s, separator)
Splits a string into a table of substrings. The separator is the character (or characters) where the split happens. If no separator is given, splits on whitespace.

```apex
import string
string.split("apple,banana,orange", ",")     // ("apple", "banana", "orange")
string.split("hello world apex", " ")        // ("hello", "world", "apex")
string.split("one two three")                // ("one", "two", "three")
string.split("word")                         // ("word",)
```

Practical example — processing user input:

```apex
import string
import os
user_input = os.input("Enter three numbers separated by commas: ")
parts = string.split(user_input, ",")
os.output("First number: {parts.1}")
os.output("Second number: {parts.2}")
os.output("Third number: {parts.3}")
```

### string.join(parts, separator)
Does the opposite of `split` — takes a table of strings and joins them into one string with a separator between each. The separator is optional — if not given, nothing is placed between parts.

```apex
import string
words = ("Hello", "World")
string.join(words, " ")       // "Hello World"
string.join(words, "-")       // "Hello-World"
string.join(words)            // "HelloWorld" (no separator)

// Join with commas
tags = ("apex", "programming", "language")
string.join(tags, ", ")       // "apex, programming, language"
```

### string.trim(s)
Removes whitespace (spaces, tabs, newlines) from the beginning and end of a string. The middle spaces are left alone.

```apex
import string
string.trim("  hello  ")      // "hello"
string.trim("   apex   lang   ")  // "apex   lang" (inner spaces kept)
string.trim("\n  text \n")    // "text"
```

Very useful when cleaning user input — users often accidentally type extra spaces:

```apex
import string
import os
name = os.input("Enter your name: ")
name = string.trim(name)      // Remove accidental spaces
os.output("Hello, {name}")
```

### string.find(s, search)
Searches for `search` inside `s` and returns the position of the first match. Returns `-1` if not found. Position starts from `0`.

```apex
import string
text = "Hello, World"
string.find(text, "World")    // 7
string.find(text, "o")        // 4 (first 'o' is at position 4)
string.find(text, "Apex")     // -1 (not found)
```

You can use the result to check if something exists in a string:

```apex
import string
email = "alice@example.com"
if string.find(email, "@") != -1
    os.output("Valid email format")
else
    os.output("Missing @ symbol")
```

### string.replace(s, old, new)
Replaces every occurrence of `old` with `new` in the string. If `old` isn't found, returns the original string unchanged.

```apex
import string
string.replace("Hello World", "World", "Apex")    // "Hello Apex"
string.replace("banana", "a", "o")                // "bonono"
string.replace("hello", "x", "y")                 // "hello" (nothing changed)

// Remove something by replacing with empty string
string.replace("remove-this", "-this", "")        // "remove"
```

## 8.4 Table Library (table)
The Table library provides functions for working with tables. Import it with `import table`.

### table.remove(t, key)
Removes an item from a table by key or index.

```apex
import table
user = (name = "Alice", age = 30, active = true)
table.remove(user, "age")
// user is now (name = "Alice", active = true)

colors = ("red", "green", "blue")
table.remove(colors, 2)
// colors is now ("red", "blue")
```

If the key or index doesn't exist, an error is thrown.

### table.has(t, key)
Returns `true` if the table has the specified key or index.

```apex
import table
user = (name = "Alice", age = 30)
table.has(user, "name")   // true
table.has(user, "city")   // false
table.has(user, 1)        // true (ordered item at position 1)
```

### table.size(t)
Returns the number of items in the table.

```apex
import table
user = (name = "Alice", age = 30)
table.size(user)          // 2

colors = ("red", "green", "blue")
table.size(colors)        // 3

empty = ()
table.size(empty)         // 0
```

### table.keys(t)
Returns a table of all keys in the table as strings. For ordered items without keys, their positions are converted to strings.

```apex
import table
user = (name = "Alice", age = 30, active = true)
keys = table.keys(user)   // ("name", "age", "active")

colors = ("red", "green", "blue")
keys = table.keys(colors) // ("1", "2", "3")
```

### table.values(t)
Returns a table of all values in the table in order.

```apex
import table
user = (name = "Alice", age = 30, active = true)
values = table.values(user)   // ("Alice", 30, true)

colors = ("red", "green", "blue")
values = table.values(colors) // ("red", "green", "blue")
```

### table.clear(t)
Removes all items from the table.

```apex
import table
user = (name = "Alice", age = 30)
table.clear(user)         // user is now ()
```

### table.copy(t)
Returns a shallow copy of the table. Changes to the copy don't affect the original.

```apex
import table
original = (name = "Alice", age = 30)
duplicate = table.copy(original)
duplicate.name = "Bob"
// original.name is still "Alice"
```

### table.merge(t1, t2)
Merges two tables into a new table. If keys conflict, values from the second table overwrite the first.

```apex
import table
t1 = (name = "Alice", age = 30)
t2 = (city = "Dubai", age = 31)
merged = table.merge(t1, t2)  // (name = "Alice", age = 31, city = "Dubai")
```