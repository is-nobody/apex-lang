# Apex Reference Manual for Developers (26.07)
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
- [1.6 Type Functions](#16-type-functions)

### 2. Operators
- [2.1 Arithmetic Operators](#21-arithmetic-operators)
- [2.2 Comparison Operators](#22-comparison-operators)
- [2.3 Logical Operators](#23-logical-operators)

### 3. If Statements
- [3.1 If Statement](#31-if-statement)
- [3.2 Elif Statement](#32-elif-statement)
- [3.3 Else Statement](#33-else-statement)
- [3.4 Ternary Statement](#34-ternary-statement)

### 4. For Loops
- [4.1 For Counter](#41-for-counter)
- [4.2 For Table Iteration](#42-for-table-iteration)
- [4.3 For Condition](#43-for-condition)
- [4.4 Break](#44-break)
- [4.5 Continue](#45-continue)

### 5. Functions
- [5.1 Function Statement](#51-function-statement)
- [5.2 Parameters](#52-parameters)
- [5.3 Return Value](#53-return-value)
- [5.4 Call](#54-call)

### 6. Imports
- [6.1 Importing an Entire File](#61-importing-an-entire-file)
- [6.2 Importing from Sub-folders](#62-importing-from-sub-folders)
- [6.3 Importing from One Sub-folder into Another](#63-importing-from-one-sub-folder-into-another)

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
| `none` | Intentional absence of a value | `x = none` |
| `number` | Numbers (Wholes and decimals) | `x = 10`, `x = 3.14` |
| `string` | Text, sequence of characters | `x = "hello"` |
| `boolean` | True or false | `x = true`, `x = false` |
| `table` | Universal container | `x = [1, 2, 3]`, `x = [name = "John"]` |

The type of a variable can change over time. A variable initially created as a `none` can later become a `string`.

For `none`, `true` and `false` use lowercase.

`//` means a comment.

### 1.1 None
In programming, you often need to explicitly say "there's nothing here." User not found. In Apex, this is called `none`.

`none` is a separate data type and the only value of that type. It means the intentional absence of any value. It's like an empty box — it exists, but it's empty.

```apex
user = none
result = none
```

Most often, `none` is used as a default value when a function returns no meaningful result, or to reset a variable's value.

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

### Quotes inside Quotes
If you need double quotes inside a string, you can't just drop them in raw — Apex will see that second quote and think "String's over!" Then everything after it becomes gibberish that breaks your code.

Here's what **doesn't** work:

```apex
inside = "He say: "I hate donuts!"" // NOPE
```

Apex reads that as: string `"He say: "` ends, then random words `I hate donuts!` floating in space, then an empty string `""`. Total chaos.

Slap a backslash `\` in front of every inner double quote. This thing is called an **escape character**. It tells Apex: "The next character is special — treat it as a regular symbol, not as code."

Here's how it actually works:

```apex
inside = "He say: \"I hate donuts!\""
```

When Apex sees `\"`, it goes: "Oh, this is just a regular double quote that should appear in the text — not the end of my string." Without those `\` backslashes, Apex would get confused and think your string ended too early. With them, everything works perfectly and you get exactly this output:

```bash
He say: "I hate donuts!"
```

### Escaping the Backslash Itself
Backslash itself is a special character, so if you actually want a backslash in your string — like in a file path — you have to escape it too. Double them up:

```apex
filePath = "C:\\Users\\John\\Documents\\resume.pdf"
```

Each `\\` is Apex reading: "Okay, the first `\` means something special is coming... oh, the next character is also `\`? Got it, you want an actual backslash printed." Without doubling them, Apex would try to interpret `\U`, `\J`, `\D` as some special characters and everything breaks. Output with double slashes in code:

```bash
C:\Users\John\Documents\resume.pdf
```

### String Interpolation
The way you combine text with variables in Apex is string interpolation — you embed variables directly inside a string by putting the variable name inside curly braces `{}`.

```apex
name = "Alice"
age = 30
greeting = "Hello, {name}"                    // "Hello, Alice"
message = "{name} is {age} years old"         // "Alice is 30 years old"
```

Apex automatically converts numbers and other values to strings when you put them inside `{}`. 

Inside the braces, you can also use numeric expressions — they are evaluated first, and then the result is converted to a string:

```apex
count = 5
total = "Total: {count * 2}"  // Total: 10
```

### Curly Braces in Strings
Escape them the exact same way — `\{` and `\}`:

```apex
String template = "Hello {0}, your balance is {1}"
// Apex tries to find variables named "0" and "1"

String template = "Hello \{0\}, your balance is \{1\}"
// Output: Hello {0}, your balance is {1}
```

### Line Breaks and Tabs
Apex has **escape sequences**.

#### Line break — `\n`
The `n` stands for *newline*.

```apex
message = "First line\nSecond line"
```

Output:

```bash
First line
Second line
```

#### Tab — `\t`
The `t` stands for *tab*.

```apex
header = "Name\tAge\tCity"
row = "Alice\t30\tLondon"
```

Output:

```bash
Name    Age    City
Alice   30     London
```

### Multiline Strings
You just hit Enter and keep typing in a regular string wrapped in double quotes `"`.

```apex
emailBody = "
    Hello Alice,

    Thank you for your purchase.

    Your order #12345 has been shipped.

    Best regards,
    The Store Team
"
```

## 1.5 Tables
A table is Apex's universal container. Think of it as a box that can hold multiple values. Need a list of names? Use a table. Need to store information about a user? Use a table. Need both in one place? Table. Tables are flexible — they work as ordered lists and key-value pairs. You can even mix both styles in the same table.

### Creating Tables
Tables are written inside square brackets `[ ]`. Values are separated by commas.

```apex
empty = []                               // empty table — nothing inside
fruits = ["apple", "banana", "orange"]   // three items
numbers = [10, 20, 30, 40]               // four numbers
mixed = [42, "hello", true]              // different types together
```

### Ordered Lists
When you list values without keys, you create an ordered list. Each value has a position — starting from 1.

```apex
colors = ["red", "green", "blue"]
// Access by position
first_color = colors[1]      // "red"
second_color = colors[2]     // "green"
third_color = colors[3]      // "blue"
```

### Key-Value Pairs
When you want to label each value with a name, use keys. Keys and values are connected with `=`. You can't use numbers as keys, because numbers are reserved and using for calling items without keys.

```apex
user = [
    name = "Alice",
    age = 30,
    active = true
]
```

Keys are written without quotes. Apex recognizes them as names, not strings. Now you can access values by their key:

```apex
user_name = user["name"]         // "Alice"
user_age = user["age"]           // 30
user_active = user["active"]     // true
```

### Working with Keys
Once a table exists, you can working with keys:

```apex
user = [name = "Alice"]
// Add a new key
user["age"] = 30
user["city"] = "Dubai"
user["active"] = true
// Now user has four keys
// [name = "Alice", age = 30, city = "Dubai", active = true]
```

Updating a value works the same way — just assign a new value to an existing key or position. If you access a non-existent key, you will get an `false`. To remove an item from a table, use the `table.remove()` function from the table library (see section 7.5).

### Mixed Tables
Tables can combine ordered items and key-value pairs in the same table. Ordered items come first, then key-value pairs:

```apex
person = ["Alice", "Manager", department = "Engineering", years = 5]
// Access ordered items by position
name = person[1]               // "Alice"
role = person[2]               // "Manager"
// Access key-value pairs by key
dept = person["department"]      // "Engineering"
experience = person["years"]     // 5
```

### Tables Inside Tables
Tables can contain other tables. This lets you build complex data structures:

```apex
company = [
    name = "Apex",
    employees = ["Alice", "Bob", "Charlie"],
    address = [
        street = "1 Sheikh Mohammed bin Rashid Boulevard",
        city = "Dubai"
    ]
]
// Access nested values
company_name = company["name"]                     // "Apex"
first_employee = company["employees"][1]            // "Alice"
city = company["address"]["city"]                     // "Dubai"
```

## 1.6 Type Functions
Apex provides two built-in functions for explicit type conversion:

| Function | What it does | Example |
|----------|--------------|---------|
| `number(x)` | Converts to number | `number("42")` → `42` |
| `string(x)` | Converts to string | `string(42)` → `"42"` |

### number()
Converts a value to number. You will get an `false` value if conversion fails.

```apex
number("42")       // 42
number("3.14")     // 3.14
number(true)       // false (conversion fails)
number(false)      // false (conversion fails)
number("hello")    // false (conversion fails)
```

### string(x)
Converts any value to its string representation.

| Input | Output |
|-------|--------|
| `42` | `"42"` |
| `3.14` | `"3.14"` |
| `none` | `"none"` |
| `true` / `false` | `"true"` / `"false"` |

```apex
string(42)      // "42"
string(3.14)    // "3.14"
string(none)    // "none"
string(true)    // "true"
string(false)   // "false"
```

### type()
Returns the name of the variable's type as a string. This is useful for debugging or checking what kind of data you are working with.

```apex
type(42)        // "number"
type("hello")   // "string"
type(none)      // "none"
type(true)      // "boolean"
type([1, 2])    // "table"
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
|-----------|--------------|
| `if` | Condition is `true` |
| `elif` | Previous conditions were `false` AND this condition is `true` |
| `else` | All previous conditions were `false` |

### Explicit Conditions Required
In Apex, conditions must be **explicitly boolean**. You cannot use variables directly as conditions (no "truthy" or "falsy" values). 

**This does NOT work:**
```apex
import os
x = 10
if x          // ERROR: If condition must be boolean, got number
    os.output("Hello")
```

**You MUST write:**
```apex
import os
x = 10
if x == true       // Correct for booleans
if x != false      // Also correct
if x > 5           // Correct because comparison returns boolean
```

Always use comparison operators (`==`, `!=`, `<`, `>`, etc.) or logical operators (`and`, `or`, `not`) to ensure your condition results in a `boolean` value.

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

### 3.4 Ternary Operator
The ternary operator is a shorthand form for simple conditional expressions. It lets you choose one of two values depending on a condition. It's not a replacement for `if-else`, but a handy tool for cases when you need to assign a value to a variable based on a simple check.

```apex
import os
temperature = 25
weather = "hot" if temperature > 20 else "cold"
os.output(weather)   // Output: hot
```

The ternary operator can only be used for short conditions. If the selection logic requires 2 or more checks, you must use regular `if-elif-else` blocks — 2 or more checks are not allowed in a ternary statement.

# 4. For Loops
## 4.1 For Statement
A `for` loop repeats code once for each item in a collection. You give it a variable and a table. The loop runs once per item, and each time the variable holds the next value.

# 4. For Loops
Sometimes you need to do the same thing many times. Print "Hello" ten times. Keep asking for input until the user types something valid. Count down from 10 to 0. Apex gives you the `for` loop with four different syntaxes to handle all these situations.

| Syntax | When to Use | Example |
|--------|-------------|---------|
| `for x = start, end` | You know the exact range | `for i = 1, 5` |
| `for k = table` | Iterate over table items | `for k = my_table` |
| `for condition` | Repeat while condition is true | `for counter <= 10` |

## 4.1 For Counter
The `for` statement creates a numeric loop. You specify a variable, a starting number, and an ending number. The loop runs once for each number in that range, including the end value.

Syntax: `for variable = start, end [, step]`, where step is optional. By default step is 1.

```apex
import os
for i = 1, 5
    os.output(i)
```

**Steps**
You can control how much the loop variable increases or decreases by adding a third number — the `step`.

```apex
import os
for i = 0, 10, 2
    os.output(i)
```

For counting down use a negative step to count backward:

```apex
import os
for i = 5, 1, -1
    os.output(i)
```

This prints 5, 4, 3, 2, 1. The loop stops when the variable goes below the `end` value.

## 4.2 For Table Iteration
You can iterate over any table using `for key = table`. This will give you access to both the key and the value inside the loop.

```apex
import os
scores = [alice = 95, bob = 82, charlie = 90]

for name = scores
    os.output("{name}: {scores[name]}")
```

## 4.3 For Condition
When you don't know how many times you need to repeat, use a condition loop. As long as the condition is `true`, the loop keeps running.

```apex
import os

counter = 1
for counter <= 5
    os.output(counter)
    counter = counter + 1
```

## 4.4 Break
`break` exits the loop immediately — same as in `while`.

```apex
import os

for i = 1, 10
    if i == 5
        break
    os.output(i)
```

## 4.5 Continue
`continue` skips the rest of the current iteration and moves to the next number.

```apex
import os
for i = 1, 5
    if i == 3
        continue
    os.output(i)
```

# 5. Functions
## 5.1 Function Statement
To create a function, use the `function` keyword, then the function name, then parentheses `( )`, then the code block.

```apex
import os

function say_hello()
    os.output("Hello!")
```

## 5.2 Parameters
Sometimes a function needs information to do its job. You put them inside the parentheses.

```apex
import os

function greet(name)
    os.output("Hello, {name}!")

greet("Friend")
```

You can have multiple parameters, separated by commas. Order matters. The first value goes to the first parameter, the second value to the second parameter, and so on.

## 5.3 Return Value
Return value is what the function sends back after it finishes. You use the `return` keyword.

```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)    // Prints: 8
```

Functions without `return` return `false` automatically. Once `return` happens, the function exits. Nothing after it runs.

### 5.4 Call
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

# 6. Imports
Imports give you the ability to use code from other files. Every import path is relative to the main file — the file you run with `apex filename.apex`.

## 6.1 Importing an Entire File
To import everything from a file in the same folder:

```apex
import os
import database

// Use items with the filename as a prefix
database.connect()
os.output(database.APP_NAME)
```

When you import a file, you must use the filename as a prefix to access its contents.

## 6.2 Importing from Sub-folders
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

## 6.3 Importing from One Sub-folder into Another
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

# Conclusion
## What's Next?
Now you know the basics of Apex! The best next step is to explore the built-in libraries—check out the **Library Reference**. Start with the must-haves: `os`, `string`, `table`.

**Remember:**

> *Apex is designed to be simple, but it's powerful with libraries.*

**Happy coding in Apex!**