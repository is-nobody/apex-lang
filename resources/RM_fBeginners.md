# Apex Reference Manual for Beginners (26.06)
This manual is written with step-by-step learning in mind and strives to be minimalistic. Each section builds on the previous ones. For the best experience, follow the order. If you see a note like "see section X" — that's a hint that you might want to go here if something feels unfamiliar. Don't skip ahead to functions or loops before you understand variables and conditions. Everything connects.

## Table of Contents
### Introduction
- [About the Project](#about-the-project)
- [Installation](#installation)
- [Your First Code](#your-first-code)

### 1. Variables & Data Types
- [1.1 Numbers](#11-numbers)
- [1.2 Booleans](#12-booleans)
- [1.3 Strings](#13-strings)
- [1.4 Tables](#14-tables)
- [1.5 Type Functions](#15-type-functions)

### 2. Operators
- [2.1 Arithmetic Operators](#21-arithmetic-operators)
- [2.2 Comparison Operators](#22-comparison-operators)
- [2.3 Logical Operators](#23-logical-operators)

### 3. If Statements
- [3.1 If Statement](#31-if-statement)
- [3.2 Elif Statement](#32-elif-statement)
- [3.3 Else Statement](#33-else-statement)

### 4. For Loops
- [4.1 For Counter](#41-for-counter)
- [4.2 For Table Iteration](#42-for-table-iteration)
- [4.3 For Condition](#43-for-condition)
- [4.4 Break](#44-break)
- [4.5 For Infinity](#45-for-infinity)
- [4.6 Continue](#46-continue)

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
Apex is a programming language designed for simplicity and cross-platform development with built-in libraries. Apex is designed to be approachable for beginners while remaining powerful for professional developers. Apex is under the MIT License. All created by one person.

### Installing
Go to the [GitHub releases](https://github.com/is-nobody/apex-lang/releases) page and find the Download section. Select the file for your operating system, download it, and run it.

After running, you'll see the Apex REPL — an interactive environment where you can type file name for executing. REPL stands for Read, Evaluate, Print, Loop. You type something, Apex runs it, shows the result, and waits for more.

### Your First Code
Paste this code into REPL:

```apex
import os
os.output("Hello, Friend")
```

You'll see the output:

```bash
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
| `number` | Numbers (Wholes and decimals) | `x = 10`, `x = 3.14` |
| `string` | Text, sequence of characters | `x = "hello"` |
| `boolean` | True or false | `x = true`, `x = false` |
| `table` | Universal container | `x = (1, 2, 3)`, `x = (name = "John")` |

The type of a variable cannot change. If a variable was created as a `string`, it can never become a `number`.

For `true` and `false` use lowercase.

`//` means a comment, it has absolutely no effect for source code and will be used only for notes inside the code.

## 1.1 Numbers
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

## 1.3 Strings
Strings are how you work with text in Apex. Names, messages, file paths, user input — anything that's words rather than numbers lives in a string. Think of a string as a sequence of characters. The word `"hello"` is five characters: `h`, `e`, `l`, `l`, `o`. A string can be one character long, a thousand characters long, or even empty.

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
Curly braces `{}` have special meaning — string interpolation. When Apex sees `{someVariable}` inside a string, it tries to swap that placeholder with an actual value. Super useful, but if you literally want to print `{curly braces}` as text, Apex will freak out thinking it's a variable that doesn't exist.

Escape them the exact same way — `\{` and `\}`:

```apex
String template = "Hello {0}, your balance is {1}"
// Apex tries to find variables named "0" and "1"

String template = "Hello \{0\}, your balance is \{1\}"
// Output: Hello {0}, your balance is {1}
```

### Line Breaks and Tabs
Sometimes you don't just need to piece a string together — you need to format it nicely: break it into lines or add indentation. Smashing Enter right into your code won't work; Apex will think the statement ended and start reading a new line of code. Instead, you use special combos called **escape sequences**.

#### Line break — `\n`
The `n` stands for *newline*. When Apex hits `\n` inside a string, it doesn't print those two characters — it inserts an actual line break:

```apex
message = "First line\nSecond line"
```

Output:

```bash
First line
Second line
```

#### Tab — `\t`
The `t` stands for *tab*. Works just like the Tab key in a text editor — inserts a nice fixed-width indent. Perfect when you want to align data into a pseudo-table or just add some structure:

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
Alright, `\n` is great, but if you're writing a big chunk of text — an email body, a template — sprinkling `\n` everywhere gets ugly fast. The string becomes a messy pile of backslashes and n's that's hard to read and even harder to edit.

Apex has a cleaner way: **multiline strings**. You just hit Enter and keep typing in a regular string wrapped in double quotes `"`. Line breaks right there in the code become actual line breaks in the output. No `\n` needed.

```apex
emailBody = "
    Hello Alice,

    Thank you for your purchase.

    Your order #12345 has been shipped.

    Best regards,
    The Store Team
"
```

Apex captures the text exactly as you typed it — line breaks, indentation, everything. The output looks exactly like what's between the quotes. Clean, readable, and no `\n` soup in sight. Just hit Enter where you want a new line.

## 1.4 Tables
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
You can skip this for now and come back later. Tables can combine ordered items and key-value pairs in the same table. Ordered items come first, then key-value pairs:

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
You can skip this for now and come back later. Tables can contain other tables. This lets you build complex data structures:

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

## 1.5 Type Conversion
Sometimes you have a value of one type but need it in another.
`os.input()` always returns a string — even if the user types `42`.
To do math with it, convert to number first.

Apex provides two conversion functions:

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

### string()
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
string(false)   // "false"
```

### type()
Returns the name of the variable's type as a string. This is useful for debugging or checking what kind of data you are working with.

```apex
type(42)        // "number"
type("hello")   // "string"
type(true)      // "boolean"
type([1, 2])    // "table"
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

Arithmetic only works with the numbers data type, you cannot add number with string, string with boolean, etc. When you perform an arithmetic operation between a whole and a decimal, the result also becomes a decimal.

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
We don't have a user, and we check: if he doesn't exist, then display output about him. But if we have user, we does nothing and moves on.

```apex
import os
user = false
if user == false
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

# 4. For Loops
Sometimes you need to do the same thing many times. Print "Hello" ten times. Keep asking for input until the user types something valid. Count down from 10 to 0. Apex gives you the `for` loop with four different syntaxes to handle all these situations.

| Syntax | When to Use | Example |
|--------|-------------|---------|
| `for x = start, end` | You know the exact range | `for i = 1, 5` |
| `for k = table` | Iterate over table items | `for k = my_table` |
| `for condition` | Repeat while condition is true | `for counter <= 10` |
| `for` | Infinite loop (use `break` to exit) | `for` |

## 4.1 For Counter
The `for` statement creates a numeric loop. You specify a variable, a starting number, and an ending number. The loop runs once for each number in that range, including the end value.

Syntax: `for variable = start, end [, step]`, where step is optional. By default step is 1.

```apex
import os
for i = 1, 5
    os.output(i)
```

This prints numbers 1 through 5. The variable `i` automatically takes each value: `1`, `2`, `3`, `4`, `5`. The loop stops when `i` exceeds the `end` value.

**Steps**
You can control how much the loop variable increases or decreases by adding a third number — the `step`.

```apex
import os
for i = 0, 10, 2
    os.output(i)
```

This prints even numbers: 0, 2, 4, 6, 8, 10. It starts at `0`, ends at `10`, and adds `2` each time.

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

Output:

```text
alice: 95
bob: 82
charlie: 90
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

This prints 1, 2, 3, 4, 5. The loop checks the condition before each iteration. When `counter` becomes 6, the condition `counter <= 5` is `false`, so the loop stops.

**Important:** You must manually update the variable in the condition, or you'll create an infinite loop!

## 4.4 Break
`break` exits the loop immediately — same as in `while`.

```apex
import os

for i = 1, 10
    if i == 5
        break
    os.output(i)
```

This prints 1, 2, 3, 4. When `i` becomes 5, `break` stops the loop entirely. Nothing after it runs for that iteration.

Use `break` when you found what you were looking for and don't need to continue.

## 4.5 For Infinity
When you just write `for` with no condition, the loop runs forever until you explicitly stop it with `break`.

```apex
import os

for
    response = os.input("Type 'quit' to exit: ")
    if response == "quit"
        break
    os.output("You typed: {response}")
```

This keeps asking for input until the user types "quit". The `break` statement immediately exits the loop.

Infinite loops are useful when:
- You don't know when the loop should end
- The exit condition is complex or checked in the middle of the loop
- You're waiting for user input or an external event

## 4.6 Continue
`continue` skips the rest of the current iteration and moves to the next number.

```apex
import os
for i = 1, 5
    if i == 3
        continue
    os.output(i)
```

This prints 1, 2, 4, 5. When `i` is 3, `continue` jumps to the next value (`4`) without running `os.output()`.

Use `continue` when you want to skip certain values but keep looping through the rest.

# 5. Functions
Imagine you have a recipe for making a sandwich. You follow the same steps every time: take two slices of bread, spread butter on one, spread jam on the other, put them together. Now imagine you had to write out those steps every single time you wanted a sandwich. That would be tedious. Instead, you give the recipe a name — `make_sandwich` — and whenever you want a sandwich, you just say that name. That's exactly what a function is: a named block of code that you can use whenever you need it.

## 5.1 Function Statement
To create a function, use the `function` keyword, then the function name, then parentheses `( )`, then the code block.

```apex
import os

function say_hello()
    os.output("Hello!")
```

This creates a function named `say_hello`. It does one thing: prints "Hello!".

Function names should describe what they do. `say_hello` is good. `task_first` is bad.
This will help someone who sees it for the first time, or even you yourself, after a long time, understand what a function means without looking at its contents.

## 5.2 Parameters
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

## 5.3 Return Value
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
total = add_and_print(5, 3)   // total becomes false (nothing returned)
// Returning — you keep the value
function add_and_return(a, b)
    return a + b
total = add_and_return(5, 3)  // total becomes 8
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
When your project grows beyond a few dozen lines, keeping everything in one file becomes painful. You need a way to split your code into smaller, manageable pieces. That's what imports give you — the ability to use code from other files. The problems with one giant file:

- Difficult navigation – scrolling through thousands of lines to find specific functions
- Hard to collaborate – multiple developers can't work on the same file without merge conflicts
- No separation of concerns – mixing database logic, business rules, and UI code in one place
- Slow development – adding features becomes riskier as the file grows

Imports solve all of this by letting you organize your code across multiple files.

### How Apex Finds Files
Every import path is relative to the main file — the file you run with `apex filename.apex`. Think of your main file as the front door. All imports are paths from that front door, not from wherever you're standing.

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

# Conclusion
## What's Next?
Now you know the basics of Apex! The best next step is to explore the built-in libraries—check out the **Library Reference**. Start with the must-haves: `os`, `string`, `table`.

Remember: Don't be afraid to experiment and make mistakes — that's the best way to learn. Happy coding in Apex!