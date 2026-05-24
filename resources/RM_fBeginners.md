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
- [1.6 Type Conversion](#16-table-conversion)

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
Apex is a programming language designed for simplicity and cross-platform development with built-in libraries. Apex is designed to be approachable for beginners while remaining powerful for professional developers. Apex is under the MIT License. All created by one person.

### Installing
Go to the [GitHub releases](https://github.com/is-nobody/apex-lang/releases) page and find the Download section. Select the file for your operating system, download it, and run it.

After running, you'll see the Apex REPL — an interactive environment where you can type file name for executing. REPL stands for Read, Evaluate, Print, Loop. You type something, Apex runs it, shows the result, and waits for more.

### Your First Code
Create a new file named `main.apex` in your current directory. Open it and write:

```apex
import os
os.output("Hello, Friend")
```

Save the file. In the REPL, type the filename to run it:

```bash
main.apex
```

You'll see the output:

```
Hello, Friend
```

Congratulations! You've successfully written and run your first Apex program.

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

Apex automatically converts numbers and other values to strings when you put them inside `{}`. 

Inside the braces, you can also use numeric expressions — they are evaluated first, and then the result is converted to a string:

```apex
import os
count = 5
os.output("Total: {count * 2}")
```

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

## 1.6 Type Conversion
Sometimes you have a value of one type but need it in another.
`os.input()` always returns a string — even if the user types `42`.
To do math with it, convert to number first.

Apex provides two conversion functions:

| Function | What it does | Example |
|----------|--------------|---------|
| `number(x)` | Converts to number | `number("42")` → `42` |
| `string(x)` | Converts to string | `string(42)` → `"42"` |

### number()
Converts a value to number. Returns `none` if conversion fails.

```apex
number("42")       // 42
number("3.14")     // 3.14
number(true)       // 1
number(false)      // 0
number("hello")    // none — not a number
number(none)       // none
```

Always check the result before calculations:

```apex
import os
value = number(os.input("Enter a number: "))

if value == none
    os.output("That's not a valid number!")
else
    os.output("Double is {value * 2}")
```

*More details about Comparison operators & If Statements in section 2.2 & 3*

### string()
Converts any value to its string representation.

```apex
string(42)      // "42"
string(true)    // "true"
string(none)    // "none"
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

Arithmetic only works with the numbers data type, you cannot add number with string, none with boolean, etc. When you perform an arithmetic operation between a whole and a decimal, the result also becomes a decimal.

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
Comparison operators have lower precedence than arithmetic. Math happens first, then comparisons.

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

## 4.2 Break
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

When `x` becomes 5, `break` fires and the loop ends immediately. Nothing after `break` runs for that iteration.

## 4.3 Continue
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

When `x` is even, `continue` skips `os.output(x)` and goes back to check the condition again.

# 5. For Loops
A `while` loop is great when you don't know how many times you need to repeat. But sometimes you know exactly how many times — "print 'Hello' 10 times" or "show all items in a list of 5 things". For these situations, Apex gives you the for loop. A `for` loop repeats code once for each item in a collection. You give it a variable and a table. The loop runs once per item, and each time the variable holds the next value.

| Situation | Use | Why |
|-----------|-----|-----|
| Loop through items in a table | `for item in table` | Natural — one item at a time |
| Repeat N times | `for i in range(1, N+1)` | You know the exact count |
| Keep asking until valid input | `while` | Don't know when user will respond |
| Wait for a file to load | `while` | Condition is unpredictable |

Think of it this way:

- While: "Keep eating while the plate has food" — you don't know how many bites
- For: "Take for 10 bites" — you know exactly how many

## 5.1 For Statement
The `for` statement loops through every item in a table. Use it when you have a collection of things to process — names, prices, users, or any list of values.

```apex
import os
shopping_list = ("milk", "bread", "eggs")
for item in shopping_list
    os.output(item)
```

Each time the loop runs, the variable (`item`) takes the next value from the table. The loop automatically stops when there are no more items.

## 5.2 Range
Need to repeat code a specific number of times? Use `range()`. The `range()` function creates a table with numbers.

```apex
import os
for i in range(1, 6)
    os.output(i)
```

This prints numbers 1 through 5. `range(1, 6)` creates the table `(1, 2, 3, 4, 5)`.

**Why 6?** `range(start, end)` goes up to but **not including** `end`. So `range(1, 6)` gives you 1,2,3,4,5 — exactly five numbers.

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

This prints 1, 2, 3, 4. When `i` becomes 5, `break` stops the loop entirely. Nothing after it runs for that iteration.

Use `break` when you found what you were looking for and don't need to continue.

## 5.4 Continue
`continue` skips the rest of the current iteration and moves to the next item.

```apex
import os
for i in range(1, 6)
    if i == 3
        continue
    os.output(i)
```

This prints 1, 2, 4, 5. When `i` is 3, `continue` jumps to the next item (`4`) without running the `os.output()`.

Use `continue` when you want to skip certain items but keep looping through the rest.

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
The `failure` block only runs when an error occurs. It's your safety net — the program takes a different path instead of crashing.

Think of it like a backup plan. You *try* to do something risky. If it works, great. If it fails, the `failure` block catches you.

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

What happens in each scenario:

| File `data.txt` exists? | What runs | Output |
|--------------------------|-----------|--------|
| Yes | `try` block | `File loaded: (contents)` then `Final content: (contents)` |
| No | `failure` block | `Could not read file — using default value` then `Final content: default content` |

Notice the key difference from crashing: the variable `file_content` always gets a value, and the program keeps running to the `os.output` at the end. No crash, no sudden stop — just a safe fallback.

## 6.3 Always Statement
Sometimes you need code that runs whether an error happened or not. That's what `always` is for — cleanup tasks, resetting values, or letting the user know the operation finished.

Imagine you're trying to read a file and display its contents. Whether the file exists or not, you want to tell the user "Done." The `always` block guarantees that message appears.

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

What happens in each scenario:

| File exists? | `try` runs? | `failure` runs? | `always` runs? |
|-----|-----|-----|-----|
| Yes | ✓ | ✗ | ✓ |
| No | ✗ | ✓ | ✓ |

The `always` block is your guarantee. Use it when something absolutely must happen — no matter what went wrong or right before it.

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

```
import os

function introduce(first_name, last_name, age)
    os.output("My name is {first_name} {last_name} and I am {age} years old")

introduce("Alice", "Smith", 30)
```

Order matters. The first value goes to the first parameter, the second value to the second parameter, and so on.

Sometimes you need a function to accept a specific data type of a variable. For this, you need to use `==` and specify the type (available is: `none`, `number`, `boolean`, `string`, `table`):

```apex
import os

function add(a == number, b == number)
    os.output(a + b)
add(5, 6)
```

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

# 9. Built-in Libraries
Apex comes with several built-in libraries. These are ready-to-use tools that solve common tasks: working with files, math, strings, networking, and more. You don't need to write everything from scratch — just import the library you need and use it.

## 9.1 OS Library (os)
The OS library lets you interact with the operating system. You can print text, read files, create folders, and more. Import it with `import os`.

### os.output(value)
Prints a value to the terminal. If no value is given, it prints `none`.

```apex
import os
os.output("Hello, Friend")     // Hello, Friend
os.output(42)                  // 42
os.output(true)                // true
os.output()                    // none
```

### os.input(prompt)
Waits for the user to type something and press Enter. Returns what they typed as a string. You can provide an optional prompt message.

```apex
import os
name = os.input("What is your name? ")
os.output("Hello, {name}")
```

If the user types `Alice`, the output is `Hello, Alice`. The value from `os.input()` is always a string — even if the user types numbers. Use `number()` to convert it if you need to do math.

### os.read(filename)
Reads the entire contents of a file and returns it as a string. Returns `none` if the file doesn't exist or can't be read.

```apex
import os
content = os.read("story.txt")
if content == none
    os.output("Could not read the file")
else
    os.output(content)
```

### os.write(filename, content)
Writes content to a file. Creates the file if it doesn't exist, overwrites it if it does. Returns `true` on success, `false` on failure.

```apex
import os
success = os.write("notes.txt", "Today I learned Apex!")
if success
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
if os.exists("config.apex")
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

Returns `none` if the path doesn't exist.

```apex
import os
info = os.stat("data.txt")
if info != none
    os.output("Size: {info.size} bytes")
    os.output("Is folder: {info.isdir}")
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

## 9.2 Math Library (math)
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
Returns the square root of a number. Returns `none` for negative numbers — you can't take the square root of a negative in real numbers.

```apex
import math
math.sqrt(25)      // 5
math.sqrt(2)       // 1.4142135623730951
math.sqrt(-1)      // none
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
Returns the logarithm of `x`. Without a base, uses the natural logarithm (base `e`). With a base, calculates the logarithm with that base. Returns `none` if `x` is zero or negative — logarithms aren't defined for those values.

```apex
import math
math.log(2.71828)       // ~1 (natural log of e)
math.log(100, 10)       // 2 (10² = 100)
math.log(8, 2)          // 3 (2³ = 8)
math.log(0)             // none
math.log(-5)            // none
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
Returns the arcsine of `x` in radians — the angle whose sine is `x`. Input must be between -1 and 1. Returns `none` if outside that range.

```apex
import math
math.asin(0)             // 0
math.asin(1)             // 1.5707963267948966 (π/2)
math.asin(2)             // none
```

### math.acos(x)
Returns the arccosine of `x` in radians — the angle whose cosine is `x`. Input must be between -1 and 1. Returns `none` if outside that range.

```apex
import math
math.acos(1)             // 0
math.acos(0)             // 1.5707963267948966 (π/2)
math.acos(2)             // none
```

### math.atan(x)
Returns the arctangent of `x` in radians — the angle whose tangent is `x`.

```apex
import math
math.atan(0)             // 0
math.atan(1)             // 0.7853981633974483 (π/4)
```

Here's the String Library section for the manual:

## 9.3 String Library (string)
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

## 9.4 Network Library (network)
The Network library lets your program communicate over the internet. You can make HTTP requests, work with URLs, create TCP connections, and more. Import it with `import network`.

### network.url_encode(s)
Converts a string into a URL-safe format by replacing special characters with percent-encoded versions. Use this when building URLs with user input that might contain spaces or symbols.

```apex
import network
network.url_encode("hello world")        // "hello%20world"
network.url_encode("name=Alice&age=30")  // "name%3DAlice%26age%3D30"
```

### network.url_decode(s)
Does the opposite of `url_encode` — converts percent-encoded characters back to their original form.

```apex
import network
network.url_decode("hello%20world")       // "hello world"
network.url_decode("name%3DAlice%26age%3D30")  // "name=Alice&age=30"
```

### network.parse_url(url)
Breaks a URL into its components and returns them as a table. The table contains these keys:

- `scheme` — the protocol (`http`, `https`, etc.)
- `host` — the domain name or IP address
- `port` — the port number (if specified in the URL)
- `path` — the path after the domain
- `query` — everything after `?` in the URL
- `fragment` — everything after `#` in the URL

Returns `none` if the URL can't be parsed.

```apex
import network
url = "https://example.com:8080/search?q=apex&lang=en#results"
parsed = network.parse_url(url)

if parsed != none
    os.output("Scheme: {parsed.scheme}")      // "https"
    os.output("Host: {parsed.host}")          // "example.com"
    os.output("Port: {parsed.port}")          // 8080
    os.output("Path: {parsed.path}")          // "/search"
    os.output("Query: {parsed.query}")        // "q=apex&lang=en"
    os.output("Fragment: {parsed.fragment}")  // "results"
```

### network.http_get(url, headers)
Sends an HTTP GET request to a URL and returns the response body as a string. The `headers` parameter is optional — you can pass a table of header key-value pairs.

```apex
import network
import os

// Simple request
response = network.http_get("https://api.example.com/data")
os.output(response)

// Request with headers
headers = (
    Authorization = "Bearer token123",
    Accept = "application/json"
)
response = network.http_get("https://api.example.com/protected", headers)
os.output(response)
```

If the request fails, the function returns an error message string starting with `HTTP Error:`, `URL Error:`, or `Error:` instead of crashing your program.

### network.http_post(url, body, headers)
Sends an HTTP POST request with a body. The `body` can be a string, a table (which gets converted to JSON), or `none`. The `headers` parameter is optional.

```apex
import network
import os

// POST with string body
response = network.http_post(
    "https://api.example.com/submit",
    "name=Alice&age=30"
)

// POST with JSON body (pass a table)
data = (name = "Alice", age = 30, active = true)
response = network.http_post(
    "https://api.example.com/users",
    data,
    (Authorization = "Bearer token123")
)
os.output(response)
```

### network.http_set_timeout(seconds)
Sets how long HTTP requests wait before giving up. The default is 30 seconds. This affects all future HTTP requests.

```apex
import network
network.http_set_timeout(10)    // Wait at most 10 seconds
response = network.http_get("https://slow-server.example.com")
```

### network.tcp_connect(host, port)
Creates a TCP connection to a server. Returns a table with connection info, or `none` if the connection fails. The returned table has these keys:

- `id` — a unique number identifying this socket (you'll need it for other socket functions)
- `connected` — `true` if connected successfully
- `host` — the host you connected to
- `port` — the port you connected to

```apex
import network
import os

socket = network.tcp_connect("example.com", 80)
if socket != none
    os.output("Connected! Socket ID: {socket.id}")
else
    os.output("Connection failed")
```

### network.socket_send(socket_id, data)
Sends data through a connected socket. Returns `true` on success, `false` on failure.

```apex
import network
socket = network.tcp_connect("example.com", 80)
if socket != none
    network.socket_send(socket.id, "Hello, server!")
```

### network.socket_receive(socket_id, bytes_count)
Receives data from a socket. The `bytes_count` parameter is optional — if not given, reads up to 4096 bytes. Returns the received data as a string, or `none` if nothing was received or an error occurred.

```apex
import network
socket = network.tcp_connect("example.com", 80)
if socket != none
    network.socket_send(socket.id, "GET / HTTP/1.0\r\n\r\n")
    response = network.socket_receive(socket.id)
    os.output(response)
```

### network.socket_close(socket_id)
Closes a socket connection. If no ID is given, closes all open sockets.

```apex
import network
socket = network.tcp_connect("example.com", 80)
// ... do something with the socket ...
network.socket_close(socket.id)      // Close specific socket
network.socket_close()               // Close all sockets
```

### network.socket_set_timeout(seconds)
Sets the timeout for all socket operations. If a socket doesn't respond within this time, it gives up instead of waiting forever.

```apex
import network
network.socket_set_timeout(5)    // 5-second timeout for socket operations
```

### network.tcp_listen(port, backlog)
Creates a TCP server that listens for incoming connections on the given port. The `backlog` parameter is optional (default is 5) — it's the maximum number of waiting connections. Returns a table with server info, or `none` on failure.

The returned table contains:
- `id` — a unique number identifying this server (you'll need it for `server_accept`)
- `port` — the port the server is listening on
- `listening` — `true` if the server started successfully

```apex
import network
import os

server = network.tcp_listen(8080)
if server != none
    os.output("Server listening on port {server.port}")
else
    os.output("Failed to start server")
```

### network.server_accept(server_id)
Waits for a client to connect to your server. When someone connects, returns a table with info about the new connection. Returns `none` if no one connected within the timeout period.

The returned table contains:
- `id` — a socket ID for communicating with this client
- `address` — the client's address as a string like `"192.168.1.5:54321"`
- `host` — the client's IP address
- `port` — the client's port

```apex
import network
import os

server = network.tcp_listen(8080)
if server != none
    os.output("Waiting for connection...")
    client = network.server_accept(server.id)
    if client != none
        os.output("Client connected from {client.address}")
        
        // Receive data from client
        data = network.socket_receive(client.id)
        os.output("Received: {data}")
        
        // Send response
        network.socket_send(client.id, "Message received!")
        
        network.socket_close(client.id)
    
    network.server_close(server.id)
```

### network.server_close(server_id)
Stops a server and closes it. If no ID is given, closes all servers.

```apex
network.server_close(123)    // Close specific server
network.server_close()       // Close all servers
```

### network.dns_lookup(hostname)
Converts a domain name (like `"google.com"`) into an IP address (like `"142.250.185.46"`). Returns `none` if the hostname can't be resolved.

```apex
import network
ip = network.dns_lookup("github.com")
os.output("GitHub IP: {ip}")    // Something like "140.82.121.3"
```

### network.ip_is_valid(ip)
Checks whether a string is a valid IPv4 or IPv6 address. Returns `true` if valid, `false` if not.

```apex
import network
network.ip_is_valid("192.168.1.1")                              // true
network.ip_is_valid("256.1.1.1")                                // false (256 is out of range)
network.ip_is_valid("2001:0db8:85a3:0000:0000:8a2e:0370:7334")  // true
network.ip_is_valid("not an ip")                                // false
```

# Conclusion
## What's Next?
Now you know the basics of Apex! You have all the tools necessary to create real programs: variables for storing data, operators for manipulating them, conditional statements and loops for controlling execution flow, functions for organizing code, and imports for structuring it.

Here are a few ideas for what to do next:

1. **Practice** — Write a calculator program, a password generator, or a simple game (like guess the number).

2. **Explore the built-in libraries** — Experiment with `os`, `math`, `string`, and `network`. Try reading a file from disk, making an HTTP request to an API, or creating a simple TCP chat.

3. **Build a project** — Combine everything you've learned. For example: a program that fetches data from an API, saves it to a file, and displays a nicely formatted report to the user.

4. **Read other people's Apex programs** — Search GitHub for Apex projects. Reading someone else's code is a great way to learn.

Remember: Apex is designed to be simple, but it's also powerful. Don't be afraid to experiment and make mistakes — that's the best way to learn. Happy coding in Apex!