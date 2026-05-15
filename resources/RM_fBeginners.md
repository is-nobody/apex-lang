# Apex Reference Manual for Beginners (26.06)
This manual is written with step-by-step learning in mind and strives to be minimalistic. Each section builds on the previous ones. For the best experience, follow the order. If you see a note like "see section X" — that's a hint that you might want to go here if something feels unfamiliar. Don't skip ahead to functions or loops before you understand variables and conditions. Everything connects.
## Table of Contents
### Introduction
- [About the Project](#about-the-project)
- [Installation](#installation)
- [Your First Code](#your-first-code)
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
- [5.2 In Operator](#52-in-operator)
- [5.3 To Operator](#53-to-operator)
- [5.4 Range](#54-range)
- [5.5 Break](#55-break)
- [5.6 Continue](#56-continue)
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
- [8.1 Import Module](#81-import-module)
- [8.2 From Import](#82-from-import)
### 9. Built-In Libraries
- [9.1 OS Library (os)](#91-os-library-os)
- [9.2 Math Library (math)](#92-math-library-math)
- [9.3 String Library (string)](#93-string-library-string)
- [9.4 Network Library (network)](#94-network-library-network)
- [9.5 UI Library (ui)](#96-ui-library-ui)
### Conclusion
- [What's Next?](#whats-next)
# Introduction
## About the Project
Apex is a programming language designed for simplicity and cross-platform development with powerful built-in libraries. Apex is designed to be approachable for beginners while remaining powerful for professional developers. Apex is under the MIT License. All created by one person.
## Installation
We'll be using Apex Code - Apex pre-installed here.

Go to the official Apex Code URL: [https://apex.org/apex-code](https://apex.org/apex-code) and navigate to the Download section and select the appropriate file for your operating system. Download it. After download, run it.

If you prefer not to install anything locally, you can try Apex in your browser: [https://apex.org/apex/try](https://apex.org/apex/try)
## Your First Code
When you run Apex Code for the first time, you already have a file with this code, but if not, copy it:
```apex
import os
os.output("Hello, Friend")
```
After that, on the right side, find & click the run button. You'll see the output:
```apex
Hello, Friend
```
Congratulations! You've successfully run your first code.
### Code Breakdown: "Hello, Friend"
`import os` — imports the built-in OS (Operating System) library. This library provides functions for interacting with the system, such as outputting text. You can learn more about available functions in the section [9.1 OS Library](#91-os-library-os).

`os.output` — calls the `output` function from the imported `os` library. This function output text to the terminal.

`("Hello, Friend")` — the parentheses contain the argument passed to the function. In this case, it's a string — a sequence of text characters. Strings in Apex are enclosed in double quotes `"..."`. 

Now something may be unclear, after reading the following sections everything will be crystal clear!
# 1. Data Types
Every variable has a name and a value. Every variable belongs to a specific data type. Apex determines the type automatically. Main data types in language:
| Type | Description | Example |
|------|-------------|---------|
| `None` | Nothing, no value | `x = none` |
| `Number` | Numbers (Wholes and decimals) | `x = 10`, `x = 3.14` |
| `String` | Text, sequence of characters | `x = "hello"` |
| `Boolean` | True or false | `x = true`, `x = false` |
| `Table` | Universal container | `x = (1, 2, 3)`, `x = (name = "John")` |

For `none`, `true`, and `false`, case doesn't matter — just write them!

`//` means a comment, it has absolutely no effect for source code and will be used only for notes inside the code.
## 1.1 None
None represents nothing — no value, empty, undefined. It's like an empty box. The box exists, but there's nothing inside. Use it as a placeholder for a future value, for resetting a variable, for optional data (like a user's middle name), or as a default state before user input.
```apex
x = none        // x exists but has no value
y = 10          // y had a value
y = none        // now y empty
```
## 1.2 Numbers
Numbers are everywhere: counting items, storing prices, tracking scores. In Apex, you don't need to worry about whether a number is a whole number or a decimal. Just write it, and Apex figures out the rest. There is no limit to numbers, you can write any numbers.
### Whole Numbers
Whole numbers are written without any other symbols. They can be positive or negative.
```apex
year = 2024
temperature = -5
```
Think of these like the numbers you use for counting: 4 apple, 2 cars.
### Decimal Numbers
Decimals are written with a dot `.`. Use them when you need precision — money, measurements, or anything with fractions.
```apex
weight = 71.5
height = 1.8
```
Apex uses a dot, not a comma. Commas have a different job — they separate items in table lists.
### Declaring with Arithmetic
You can declare variables and do math with them at the same time. Once a variable holds a number, you can use it in calculations just like a regular number. What you can do with numbers:
- Add: 10 + 5 = 15
- Subtract: 10 - 5 = 5
- Multiply: 10 * 5 = 50
- Divide: 10 / 5 = 2
- Modulo: 10 % 3 = 1
```apex
x = 10
y = 3.5
sum = x + y            // 13.5
```
How it works:
1. `x = 10` creates a variable `x` with the value `10`
2. `y = 3.5` creates a variable `y` with the value `3.5`
3. `sum = x + y` adds `x` and `y`, stores the result in `sum`

*More details about Arithmetic operators in section 2.1*
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
Whenever you use comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`), the result is always a boolean. Booleans also come from logical operations that combine values:
```apex
is_weekend = true
is_holiday = false
can_sleep_in = is_weekend or is_holiday    // true
```
*More details about Comparison & Logical operators in section 2.2 & 2.3*
### Booleans as Switches
One common pattern is using booleans as switches that can be toggled on and off:
```apex
lights_on = true

// Later, you can flip the value
lights_on = not lights_on      // now false
lights_on = not lights_on      // now true again
```
You'll use them constantly with `if` statements (section 3) to make decisions.
## 1.4 Strings
Strings are how you work with text in Apex. Names, messages, file paths, user input — anything that's words rather than numbers lives in a string. Think of a string as a sequence of characters. The word `"hello"` is five characters: `h`, `e`, `l`, `l`, `o`. A string can be one character long, a thousand characters long, or even empty.
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
The way you combine text with variables in Apex is string interpolation — you embed variables directly inside a string by putting the variable name inside curly braces `{}`.
```apex
name = "Alice"
age = 30
greeting = "Hello, {name}"                    // "Hello, Alice"
message = "{name} is {age} years old"         // "Alice is 30 years old"
```
This works with any value, not just strings:
```apex
score = 95
price = 19.99
is_active = true

result = "Score: {score}, Price: {price}, Active: {is_active}"
// "Score: 95, Price: 19.99, Active: true"
```
Apex automatically converts numbers and other values to text when you put them inside `{}`. Inside them, you can only specify the name of the variable, but not the expression.

More advanced operations — like finding words, changing case, or splitting text — are available through [9.3 String Library](#93-string-library-string). You'll learn it later.
## 1.5 Tables
A table is Apex's universal container. Think of it as a box that can hold multiple values. Need a list of names? Use a table. Need to store information about a user? Use a table. Need both in one place? Table. Tables are flexible — they work as ordered lists and key-value pairs. You can even mix both styles in the same table.
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
You can skip this for now and come back later. Tables can combine ordered items and key-value pairs in the same table. Ordered items come first, then key-value pairs:
```apex
person = ("Alice", "Manager", department = "Engineering", years = 5)
// Access ordered items by position
name = person.1               // "Alice"
role = person.2               // "Manager"
// Access key-value pairs by key
dept = person.department      // "Engineering"
experience = person.years     // 5
```
### Tables Inside Tables
You can skip this for now and come back later. Tables can contain other tables. This lets you build complex data structures:
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
company_name = company.name                     // "Apex"
first_employee = company.employees.1            // "Alice"
city = company.address.city                     // "Dubai"
```
# 2. Operators
Operators are symbols that tell Apex to perform specific actions on values. Think of them like verbs in a sentence — they make things happen.
## 2.1 Arithmetic Operators
Arithmetic operators work with numbers. They do exactly what you learned in math class.
| Operator | Name | Example | Result |
|----------|------|---------|--------|
| `+` | Addition | `5 + 3` | `8` |
| `-` | Subtraction | `10 - 4` | `6` |
| `*` | Multiplication | `7 * 6` | `42` |
| `/` | Division | `15 / 4` | `3.75` |
| `%` | Modulo (remainder) | `15 % 4` | `3` |

Arithmetic only works with the numbers data type, you cannot add number with string, none with boolean, etc. When you perform an arithmetic operation between an whole and a decimal, the result also becomes a decimal.
### Addition (`+`)
Adds two numbers together.
```apex
price = 25
tax = 3.75
total = price + tax        // 28.75
```
### Subtraction (`-`)
Subtracts one number from another.
```apex
balance = 100
withdrawal = 30
remaining = balance - withdrawal    // 70
```
### Multiplication (`*`)
Multiplies numbers.
```apex
width = 5
height = 3
area = width * height       // 15
```
### Division (`/`)
Divides one number by another. You can't divide by zero.
```apex
total = 100
people = 4
share = total / people      // 25
// Division always gives you a decimal if there's a remainder
7 / 2 = 3.5     // Not 3 — Apex keeps the decimal
```
### Modulo (`%`)
Modulo gives you the remainder after division or to check parity. Example: when we divide 10 candies among 3 people, there'll be 1 left.
```apex
10 % 3 = 1     // 10 divided by 3 is 3 with 1 left over
15 % 4 = 3     // 15 divided by 4 is 3 with 3 left over
20 % 5 = 0     // 20 divided by 5 is exactly 4 with 0 left over
```
### Operator Precedence
Just like in math, multiplication and division happen before addition and subtraction:
```apex
result = 2 + 3 * 4      // 14, not 20
// 3 * 4 happens first (12), then 2 + 12 = 14
```
To change the order, use parentheses:
```apex
result = (2 + 3) * 4    // 20, because parentheses happen first
```
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
### Equal To (`==`)
Checks if two values are the same. Use `==` for comparison, not `=` (which is assignment).
```apex
5 == 5        // true
10 == 3       // false

// Different types
5 == "5"      // false — number vs string
```
### Not Equal To (`!=`)
Checks if two values are different.
```apex
5 != 3        // true
10 != 10      // false

user = none
user != none              // false — user IS none
```
### Less Than (`<`) and Greater Than (`>`)
```apex
10 > 5        // true
10 < 5        // false
```
### Less Than or Equal (`<=`) and Greater Than or Equal (`>=`)
```apex
10 >= 5       // true
10 <= 5       // false

69 >= 69      // true
69 <= 69      // true
```
### String Comparisons
You can compare strings, but only for similarity, using `==` and `!=`:
```apex
"Bob" == "Bob"     // true
"Bob" != "Bob"     // false
```
### Operator Precedence
Comparison operators have lower precedence than arithmetic. Math happens first, then comparisons:
```apex
result = 10 - 2 == 8    // true
```
## 2.3 Logical Operators
Logical operators combine boolean values (`true` or `false`) to create more complex conditions.
| Operator | Name | What It Does | Example | Result |
|----------|------|--------------|---------|--------|
| `and` | AND | Both sides must be true | `true and true` | `true` |
| `or` | OR | At least one side must be true | `true or false` | `true` |
| `not` | NOT | Reverses the value | `not true` | `false` |
### AND (`and`)
Both conditions must be true for the result to be true.
```apex
age = 25
has_license = true
can_drive = age >= 18 and has_license     // true
```
### OR (`or`)
At least one condition must be true for the result to be true.
```apex
day = "Saturday"
is_holiday = false
can_relax = day == "Saturday" or is_holiday     // true
```
### NOT (`not`)
Reverses a boolean value.
```apex
is_raining = false
can_walk = not is_raining        // true
```
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
Every program makes decisions. Should this user get access? Is the score high enough? Does the file exist? If statements are how you tell Apex to make these decisions. Without if statements, your program runs the same way every time — like a train on a track. With if statements, it becomes a car that can turn left or right depending on what happens.
| Statement | When It Runs |
|-----------|---------------|
| `if` | Condition is `true` |
| `elif` | Previous conditions were `false` AND this condition is `true` |
| `else` | All previous conditions were `false` |

Apex does not treat `none` as `false`. Always compare with `== none` or `!= none`.
## 3.1 If Statement
We don't have a user, and we check: if he doesn't exist, then display output about him. But if we have user, we does nothing and moves on.
```apex
import os
user = none
if user == none
    os.output("No user found")
```
## 3.2 Elif Statement
Sometimes one condition isn't enough. What if you want to check multiple possibilities? Use `elif` — short for "else if".
```apex
import os
score = 85
if score >= 90
    os.output("Grade: A")
elif score >= 80
    os.output("Grade: B")
elif score >= 70
    os.output("Grade: C")
elif score >= 60
    os.output("Grade: D")
```
Apex checks conditions in order from top to bottom. As soon as one condition is `true`, it runs that block and skips the rest.
- Checks: is it >= 90? No — move on
- Checks: is it >= 80? Yes — prints "Grade: B" and stops
- The remaining elif blocks are never checked
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
Since `score` is 55, the `if` condition is `false`, so the `else` block runs.
# 4. While Loops
Sometimes you need to do the same thing many times. Print "Hello" ten times. Keep asking for input until the user types something valid. Count down from 10 to 0. A while loop lets you repeat code while a condition is true. As soon as the condition becomes false, the loop stops. Imagine you're told: "While your plate is not empty, take another bite."
You check: is the plate empty?
- If false (it still has food) = take a bite = check again
- If true (empty) = stop eating

That's exactly how a while loop works. You check a condition. If it's true, you do something. Then you check again. And again. Until the condition becomes false.
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
Output:
```
1
2
3
4
5
```
### Common Mistake: The Infinite Loop
If the condition never becomes `false`, the loop runs forever. This is called an infinite loop. Your program will never stop.
```apex
import os
x = 1
while x <= 10
    os.output(x)
    // Forgot to change x — it stays 1 forever!
```
This prints `1` again and again until you force the program to stop. Always make sure something inside the loop changes the condition:
```apex
import os
x = 1
while x <= 10
    os.output(x)
    x = x + 1   // Now x increases — loop will end
```
### 4.2 Break
`break` immediately exits the loop, no matter what the condition says. Use it when you need to stop early.
```apex
import os
x = 1
while x <= 10
    if x == 5
        break           // Stop completely when x reaches 5
    os.output(x)
    x = x + 1
```
Output:
```
1
2
3
4
```
When `x` becomes 5, `break` fires and the loop ends immediately. Nothing after `break` runs for that iteration.
### 4.3 Continue
`continue` skips the rest of the current iteration and jumps to the next check of the condition.
```apex
import os
x = 0
while x < 10
    x = x + 1
    if x % 2 == 0
        continue        // Skip output even numbers
    os.output(x)
```
Output:
```
1
3
5
7
9
```
When `x` is even, `continue` skips `os.output(x)` and goes back to check the condition again.
# 5. For Loops
A `while` loop is great when you don't know how many times you need to repeat. But sometimes you know exactly how many times — "print 'Hello' 10 times" or "show all items in a list of 5 things". For these situations, Apex gives you the for loop. It's a cleaner way to count through a range of values.
| Situation | What to Use | Why |
|-----------|-------------|-----|
| "Repeat 10 times" | `for` | You know the exact count |
| "Loop through items 1 to 5" | `for` | You know the range |
| "Keep asking until user says quit" | `while` | You don't know when |
| "Wait for file to load" | `while` | Condition is unpredictable |
Think of it this way:
- While: "Keep eating while the plate has food" — you don't know how many bites
- For: "Take for 10 bites" — you know exactly how many
## 5.1 For Statement
A `for` loop repeats code a specific number of times. You give it a counter variable, a start value, and an end value.
```apex
import os
for counter = 1 to 5
    os.output(counter)
```
Output:
```
1
2
3
4
5
```
What happens:
1. `counter = 1` — start here
2. Is counter ≤ 5? True — run the code and print `1`
3. Move to next number (2) — repeat
4. When counter becomes 6, stop
## 5.2 In Operator
The `in` keyword lets you loop through items in a table. The loop runs once for each item.
```apex
import os
colors = ("red", "green", "blue")
for color in colors
    os.output(color)
```
Output:
```
red
green
blue
```
Each time the loop runs, `color` holds the next value from the table.
```apex
import os
shopping_list = ("milk", "eggs", "bread")
for item in shopping_list
    os.output("Buy: {item}")
```
Output:
```
Buy: milk
Buy: eggs
Buy: bread
```
## 5.3 To Operator
The `to` keyword creates a number range. Apex counts from start to end. Counting up:
```apex
import os
for i = 1 to 3
    os.output("Number {i}")
```
Output:
```
Number 1
Number 2
Number 3
```
Apex automatically figures out whether to count up or down based on the start and end values.
## 5.4 Range
Sometimes you need to skip numbers. The `range` keyword lets you set the step size. Even numbers from 0 to 10:
```apex
import os
for i = 0 to 10 range 2
    os.output(i)
```
Output:
```
0
2
4
6
8
10
```
## 5.5 Break
`break` exits the loop immediately — same as in `while`.
```apex
import os
for i = 1 to 10
    if i == 5
        break
    os.output(i)
```
Output:
```
1
2
3
4
```
When `i` becomes 5, `break` stops the loop. Nothing after it runs.
## 5.6 Continue
`continue` skips the rest of the current iteration and moves to the next number.
```apex
import os
for i = 1 to 5
    if i == 3
        continue
    os.output(i)
```
Output:
```
1
2
4
5
```
When `i` is 3, `continue` jumps to `i = 4` without output.
# 6. Error Handling
Imagine you're building a program that reads a file from the user's computer. What happens if the file doesn't exist? Or if you're dividing by zero based on user input? Without error handling, your entire program crashes. You need a way to try something risky, and catch the problem without crashing. That's exactly what error handling gives you.
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

In this example:
- If `data.txt` exists, everything runs normally
- If the file is missing, the `failure` block runs instead
- Your program keeps going either way

## 6.2 Failure Statement
The `failure` block only runs when an error occurs. It's your safety net.
```apex
function divide_safe(a, b)
    try
        return a / b
    failure
        os.output("Warning: division by zero attempted")
        return 0

result = divide_safe(10, 0)   // No crash — returns 0
result = divide_safe(10, 5)   // Returns 2 normally
```

## 6.3 Always Statement
Sometimes you need code that runs whether an error happened or not. That's what `always` is for — cleanup tasks like closing files, releasing resources, or logging.

```apex
try
    connection = database.connect()
    connection.query("UPDATE users SET active = true")
failure
    os.output("Database error occurred")
always
    // This runs no matter what
    if connection != none
        connection.close()
        os.output("Connection closed")
```

# 7. Functions
Imagine you have a recipe for making a sandwich. You follow the same steps every time: take two slices of bread, spread butter on one, spread jam on the other, put them together. Now imagine you had to write out those steps every single time you wanted a sandwich. That would be tedious. Instead, you give the recipe a name — `make_sandwich` — and whenever you want a sandwich, you just say that name. That's exactly what a function is: a named block of code that you can use whenever you need it.
## 7.1 Function Statement
To create a function, use the `function` keyword, then the function name, then parentheses `( )`, then the code block.
```apex
import os

function say_hello()
    os.output("Hello!")
```
This creates a function named `say_hello`. It does one thing: prints "Hello!".

Function names should describe what they do. `say_hello` is good. `task_first` is bad.
This will help someone who sees it for the first time, or even you yourself, after a long time, understand what a function means without looking at its contents.
## 7.2 Parameters
Sometimes a function needs information to do its job. A `greet` function needs to know who to greet. A `multiply` function needs to know which numbers to multiply. Parameters are placeholders for that information. You put them inside the parentheses.
```apex
import os

function greet(name)
    os.output("Hello, {name}!")
```
Here, `name` is a parameter. It's like a blank to fill in when you use the function.
```apex
greet("Alice")    // Prints: Hello, Alice!
greet("Bob")      // Prints: Hello, Bob!
```
How it works:
1. You call `greet("Alice")`
2. Apex takes the value `"Alice"` and puts it into the parameter `name`
3. The function runs with `name = "Alice"`

You can have multiple parameters, separated by commas:
```apex
import os

function introduce(first_name, last_name, age)
    introduce("Alice", "Smith", 30)
    os.output("My name is {first_name} {last_name} and I am {age} years old")
// Prints: My name is Alice Smith and I am 30 years old
```
Order matters. The first value goes to the first parameter, the second value to the second parameter, and so on.
```apex
function divide(a, b)
    os.output(a / b)
divide(10, 2)    // Prints: 5 — a is 10, b is 2
divide(2, 10)    // Prints: 0.2 — a is 2, b is 10
```
Functions can have no parameters.
## 7.3 Return Value
Some functions just do something — like `os.output()` prints text. Other functions calculate something and give it back to you.

Return value is what the function sends back after it finishes. You use the `return` keyword.
```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)    // Prints: 8
```

This function doesn't print anything. It gives back the sum.

Think of it like a vending machine:
- You put money in (parameters)
- The machine works (function does its job)
- The machine gives you a snack (return value)

Why return instead of just output? Because output shows text on screen. Returning gives you a value you can use later.
```apex
// Output — you lose the value
function add_and_print(a, b)
    os.output(a + b)
total = add_and_print(5, 3)   // total becomes none (nothing returned)
// Returning — you keep the value
function add_and_return(a, b)
    return a + b
total = add_and_return(5, 3)  // total becomes 8
```
Functions without `return` return `none` automatically.
```apex
import os

function just_print()
    os.output("Hello")
value = just_print()   // value is none
```
Once `return` happens, the function exits. Nothing after it runs.
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
When your project grows beyond a few dozen lines, keeping everything in one file becomes painful. You need a way to split your code into smaller, manageable pieces. That's what imports give you — the ability to use code from other files. The problems with one giant file:
- Difficult navigation – scrolling through thousands of lines to find specific functions
- Hard to collaborate – multiple developers can't work on the same file without merge conflicts
- No separation of concerns – mixing database logic, business rules, and UI code in one place
- Slow development – adding features becomes riskier as the file grows

Imports solve all of this by letting you organize your code across multiple files.
### How Apex Finds Files
Every import path is relative to the main file — the file you run with `apex filename.apex`. Think of your main file as the front door. All imports are paths from that front door, not from wherever you're standing.
## 8.1 Importing an Entire File
To import everything from a file in the same folder:
```apex
import database
import config

// Use items with the filename as a prefix
database.connect()
os.output(config.APP_NAME)
```
When you import a file, you must use the filename as a prefix to access its contents.
### Importing from Sub-folders
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
### Importing from One Sub-folder into Another
Here's where beginners often get confused. You have this structure:
```
my_project/
├── main.apex
├── helpers/
│   └── math.apex
└── features/
    └── calculator.apex
```
You want to use `math.apex` inside `calculator.apex`. What path do you use? Always write the path as if you were importing from `main.apex`.
```apex
// Inside features/calculator.apex
import helpers.math         // Same as you would in main.apex!
```
Apex always starts looking from the main file's folder. This keeps your imports consistent — no matter how deep your folder structure gets, you always know exactly how to import any file.
### What Gets Imported
When you import a file, you get everything from it:
- All functions
- All variables
## 8.2 Importing Specific Items Only
Sometimes you don't want to import everything, or you want to avoid typing the filename prefix. Use `:` to import only what you need:
```apex
// Import only specific items
import os: output
// Use them directly without the filename prefix
output("Hello, Friend")
```
Using a prefix is not ​​mandatory even in this case, you can just use output without os.

You can also import specific items from sub-folders:
```apex
import utils.math: add, multiply
result = utils.math.add(5, 3)        // 8
```
Always use the full filename prefix to tell Apex which item you mean.
### What Gets Imported
When you importing a file with specific items only, you get only functions & variables specified after the colon.
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
## 9.5 UI Library (ui)
TODO
# Conclusion
## What's Next?
You've learned the core of Apex: variables, conditions, loops, functions, and imports. Now you can:
- **Build scripts** — automate your computer with the OS library
- **Create applications** — explore the UI library
- **Make games** — explore the Apex Engine
- **Share your code** — others can import your files

The best way to learn more is to start building. Open Apex Code, try something small, and when you get stuck — come back here. The [Built-In Libraries](#9-built-in-libraries) section is your main helper.