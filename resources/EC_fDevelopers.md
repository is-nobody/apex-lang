# Apex Express Course for Developers (26.09)
This manual is minimalistic. Each section builds on the previous ones. For the best experience, follow the order.

## Table of Contents
### Introduction
- [About the Project](#about-the-project)
- [Setting up workspace](#setting-up-workspace)
- [CLI Commands](#cli-commands)

### 1. Data Types
- [1.1 None](#11-none)
- [1.2 Numbers](#12-numbers)
- [1.3 Booleans](#13-booleans)
- [1.4 Strings](#14-strings)
- [1.5 Tables](#15-tables)
- [1.6 Constants](#16-constants)
- [1.7 Type Functions](#17-type-functions)

### 2. Operators
- [2.1 Arithmetic Operators](#21-arithmetic-operators)
- [2.2 Comparison Operators](#22-comparison-operators)
- [2.3 Logical Operators](#23-logical-operators)

### 3. If Statements
- [3.1 If Statement](#31-if-statement)
- [3.2 Else-If Statement](#32-elseif-statement)
- [3.3 Else Statement](#33-else-statement)
- [3.4 Ternary Operator](#34-ternary-operator)

### 4. Match
- [4.1 Match Statement](#41-match-statement)
- [4.2 Case Patterns](#42-case-patterns)
- [4.3 Default Case](#43-default-case)
- [4.4 Rules and Restrictions](#44-rules-and-restrictions)

### 5. For Loops
- [5.1 For Counter](#51-for-counter)
- [5.2 For Table Iteration](#52-for-table-iteration)
- [5.3 For Condition](#53-for-condition)
- [5.4 Break](#54-break)
- [5.5 Continue](#55-continue)

### 6. Functions
- [6.1 Function Statement](#61-function-statement)
- [6.2 Parameters](#62-parameters)
- [6.3 Return Value](#63-return-value)
- [6.4 Call](#64-call)

### 7. Imports
- [7.1 Importing an Entire File](#71-importing-an-entire-file)
- [7.2 Importing from Sub-folders](#72-importing-from-sub-folders)
- [7.3 Importing from One Sub-folder into Another](#73-importing-from-one-sub-folder-into-another)
- [7.4 Aliasing](#74-aliasing)

### Conclusion
- [What's Next?](#whats-next)

# Introduction
## About the Project
Apex is a programming language built for speed, power, simplicity, clarity, readability, and modernity. It's under the MIT License. Created by one person.

## Setting up workspace
### Installing
1. Download the interpreter from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the interpreter for your OS.

### Testing the interpreter
Run interpreter and paste this code into REPL:

```apex
import os
os.output("Hello, World!")
```

# CLI Commands
The `apex` executable is more than just a script runner. It ships with a set of commands for building, compiling, disassembling, and tuning execution. This section is a quick reference — you'll use most of these as you grow with the language.

| Command                              | What it does                           |
|--------------------------------------|----------------------------------------|
| `apex <file.apex>`                   | Run a source file directly             |
| `apex version`                       | Show version, compiler, platform, time |
| `apex build <file.apex>`             | Bundle into a standalone executable    |
| `apex build <os> <arch> <file.apex>` | Cross-build for another platform       |
| `apex compile <file.apex>`           | Compile to `.apexc` bytecode           |
| `apex emit <file.apex>`              | Disassemble bytecode to the terminal   |
| `apex jit on <file.apex>`            | Run with the JIT enabled               |
| `apex jit off <file.apex>`           | Run with the JIT disabled              |

## Running a File
The most common use — just point Apex at a source file:

```
apex hello.apex
```

Everything after the filename is passed to your script and is available through `os.args` (see the module reference).

## `apex version`
Prints the interpreter version together with build details. Useful when reporting bugs or checking which compiler built your binary.

```
apex version
```

Example output:

```
Apex 26.09 [GCC 15.2.0] on Linux x86-64
```

If the build includes the JIT, the word `JIT` appears after the version:

```
Apex 26.09 JIT [GCC 15.2.0] on Linux x86-64
```

The line shows, in order:

1. Apex version (`26.09`)
2. Whether the JIT is compiled in (`JIT`)
3. The C compiler and its version
4. The platform and architecture

## `apex build`
Compiles your source, then wraps it into a **standalone executable** that runs without the `apex` binary. This is the command you use to ship your program.

```
apex build hello.apex
```

This produces a file named after your script and the current platform, for example:

```
hello_x86-64_linux
```

### Cross-building
You can build for another platform by naming the target OS and architecture:

```
apex build windows x86-64 hello.apex
apex build linux  arm64  hello.apex
apex build macos  arm64  hello.apex
```

Supported values:

| Option       | Values                      |
|--------------|-----------------------------|
| OS           | `windows`, `linux`, `macos` |
| Architecture | `x86-64`, `arm64`           |

Cross-building requires the matching **stub** binary (`apex_26.09_<arch>_<os>[.exe]`) to sit next to your `apex` executable. The stub is the apex binary itself.

## `apex compile`
Compiles a source file to Apex bytecode and saves it as `.apexc`. This is the intermediate format used by `apex build`, and you can keep it around if you want to ship bytecode instead of source.

```
apex compile hello.apex
```

The `.apexc` file contains the full compiled chunk: instructions, constants, globals, function metadata, and the interned string pool. It can be run directly by the loader without re-parsing the source.

## `apex emit`
Disassembles a source file and prints the generated bytecode to the terminal. This is a **learning and debugging** tool — you get to see exactly what the compiler produces for your code.

```
apex emit hello.apex
```

The output has several parts:

1. A header line with the file name and summary counts.
2. The instruction stream, with each function's start marked.
3. A `constants` section.
4. A `globals` section.
5. A `functions` section with addresses, arities, and register counts.

Each instruction is printed with its offset, opcode, and a human-readable form of its operands. Registers are shown as `R0`, `R1`, etc., string constants are quoted, and jump targets are shown as addresses.

Example fragment:

```
   0  LOAD_NUM_IMM       R0 <- 10
   1  LOAD_NUM_IMM       R1 <- 20
   2  ADD                R2 <- R0 + R1
   3  STORE_GLOBAL       R2 -> sum
```

`apex emit` is the fastest way to understand how expressions, loops, and function calls turn into bytecode.

## `apex jit on` / `apex jit off`
Controls the **JIT** (just-in-time compiler) at runtime. The JIT compiles numeric-heavy functions to native machine code, which can make tight loops significantly faster.

```
apex jit on benchmark.apex
apex jit off benchmark.apex
```

- `on` — enable the JIT for this run.
- `off` — disable the JIT for this run.

Both forms accept the script name last, and everything after it is passed to your script as command-line arguments, just like the plain `apex <file.apex>` form.

If your build of Apex was compiled without JIT support, `apex jit` prints an error and exits.

# 1. Data Types
Apex determines the type automatically. Main data types in language:

| Type      | Description                    | Example                                  |
|-----------|--------------------------------|------------------------------------------|
| `none`    | Intentional absence of a value | `x = none`                               |
| `number`  | Numbers (Wholes and decimals)  | `x = 10`, `x = 3.14`                     |
| `string`  | Text, sequence of characters   | `x = "hello"`                            |
| `boolean` | True or false                  | `x = true`, `x = false`                  |
| `table`   | Universal container            | `x = [1, 2, 3]`, `x = ["name" = "John"]` |

The type of a variable can change over time. A variable initially created as a `none` can later become a `string`.

For `none`, `true` and `false` use lowercase.

`//` means a comment.

## 1.1 None
`none` is a separate data type and the only value of that type. It means the intentional absence of any value.

```apex
result = none
```

Most often, `none` is used as a default value when a function returns no meaningful result.

## 1.2 Numbers
In Apex, you don't need to worry about whether a number is a whole number or a decimal. Apex figures out the rest.

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

### Scientific Notation
For very large or very small numbers, use scientific notation with `e` or `E`. The part after `e` is the power of 10.

```apex
distance = 1.5e8     // 150000000 — 1.5 × 10^8
mass = 9.1e-31       // 0.00000000000000000000000000000091 — 9.1 × 10^-31
count = 2E+5         // 200000 — 2 × 10^5
```

The exponent can be positive or negative, and you can use `+` or `-` sign. Both lowercase `e` and uppercase `E` work the same way.

### Declaring with Arithmetic
You can declare variables and do math with them at the same time.

```apex
x = 10
y = 3.5
sum = x + y  // 13.5
```

## 1.3 Booleans
Booleans represent one of two possible values: `true` or `false`. You can create booleans in two ways: directly or through comparisons.

Direct assignment:

```apex
is_active = true
has_permission = false
```

Through comparisons:

```apex
age = 25
is_adult = age > 18  // true — because 25 is greater than 18
```

Whenever you use comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`), the result is always a boolean.

## 1.4 Strings
### Creating Strings
In Apex, strings are written inside double `""` or single `''` quotes:

```apex
name = 'Alice'
empty = ""
```

### Escape Sequences
Apex has **escape sequences**.

#### Double Quote — `\"`
If you need double quotes inside a string, use backslash before quota.

```apex
inside = "He say: \"I hate donuts!\""
```

#### Single Quote — `\'`
If you use single quotes for a string, you can escape a single quote inside it the same way. You can also escape a single quote inside a double-quoted string.

```apex
single = 'It\'s a nice day'
double = "It\'s a nice day"
```

#### Line break — `\n`
The `n` stands for *newline*.

```apex
message = "First line\nSecond line"
```

#### Tab — `\t`
The `t` stands for *tab*.

```apex
header = "Name\tAge\tCity"
row = "Alice\t30\tLondon"
```

#### Carriage Return — `\r`
The `r` stands for *carriage return*. It moves the cursor back to the start of the line without moving to the next one.

```apex
progress = "Loading...\rDone!     "
```

#### Octal Escape — `\NNN`
You can write any character by its octal code using up to three octal digits (0–7) after a backslash.

```apex
bell = "\007"      // bell character
esc  = "\033"      // escape character
null = "\0"        // null character
```

#### Escaping the Backslash Itself
Backslash itself is a special character, so if you actually want a backslash in your string — like in a file path — you have to escape it too. Double them up:

```apex
filePath = "C:\\Users\\John\\Documents\\resume.pdf"
```

#### Unknown Escapes Are Errors
If you use a backslash followed by a character that is not a known escape, Apex reports an error instead of silently ignoring it.

```apex
bad = "\q"  // ERROR: Unsupported symbol for escape sequence: '\q'
```

### String Interpolation
The way you combine text with variables in Apex is string interpolation — you embed variables directly inside a string by putting the variable name inside curly braces `{}`.

```apex
name = "Alice"
age = 30
message = "{name} is {age} years old"  // "Alice is 30 years old"
```

Apex automatically converts numbers and other values to strings when you put them inside `{}`. 

Inside the braces, you can also use numeric expressions — they are evaluated first, and then the result is converted to a string:

```apex
count = 5
total = "Total: {count * 2}"  // total: 10
```

### Curly Braces in Strings
Escape them the exact same way — `\{` and `\}`:

```apex
template = "Hello {user}, your balance is {count}"
// apex tries to find variables named "user" and "count"

template = "Hello \{user\}, your balance is \{count\}"
// output: Hello {user}, your balance is {count}
```

### Multiline Strings
You just hit Enter and keep typing in a regular string.

```apex
email = "
    Hello,

    Thank you for your purchase.

    Your order has been shipped.

    Best regards,
    The Store Team
"
```

## 1.5 Tables
A table is Apex's universal container. Tables are flexible — they work as ordered lists and key-value pairs. You can even mix both styles in the same table.

### Creating Tables
Tables are written inside square brackets `[]`. Values are separated by commas.

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
// access by position
first_color = colors[1]      // "red"
second_color = colors[2]     // "green"
third_color = colors[3]      // "blue"
```

### Key-Value Pairs
When you want to label each value with a name, use keys. Keys and values are connected with `=`. You can't use numbers as keys, because numbers are reserved and using for calling items without keys.

```apex
user = [
    "name" = "Alice",
    "age" = 30,
    "active" = true
]
```

Keys are written with quotes. Now you can access values by their key:

```apex
user_name = user["name"]         // "Alice"
user_age = user["age"]           // 30
user_active = user["active"]     // true
```

### Working with Keys
Once a table exists, you can working with keys:

```apex
user = ["name" = "Alice"]
// add a new keys
user["age"] = 30
user["city"] = "Dubai"
// now user has three keys
```

Updating a value works the same way — just assign a new value to an existing key or position. If you access a non-existent key, you will get an `none`. To remove an item from a table, use the `table.remove()` function from the table library (see [Library Reference](Library_Reference.md)).

### Mixed Tables
Tables can combine ordered items and key-value pairs in the same table. Ordered items come first, then key-value pairs:

```apex
person = ["Alice", "Manager", "department" = "Engineering", "years" = 5]
// access ordered items by position
name = person[1]              // "Alice"
role = person[2]              // "Manager"
// access key-value pairs by key
dept = person["department"]   // "Engineering"
experience = person["years"]  // 5
```

### Tables Inside Tables
Tables can contain other tables. This lets you build complex data structures:

```apex
company = [
    "name" = "Apex",
    "employees" = ["Alice", "Bob", "Charlie"],
    "address" = [
        "street" = "1 Sheikh Mohammed bin Rashid Boulevard",
        "city" = "Dubai"
    ]
]
// access nested values
company_name = company["name"]                  // "Apex"
first_employee = company["employees"][1]        // "Alice"
city = company["address"]["city"]               // "Dubai"
```

## 1.6 Constant
Constants makes a variable read-only after declaration.

```apex
constant APP_NAME = "MyApp"
APP_NAME = none  // ERROR: Cannot reassign constant 'APP_NAME'
```

## 1.7 Type Functions
Apex provides two built-in functions for explicit type conversion:

| Function    | What it does                      | Example                 |
|-------------|-----------------------------------|-------------------------|
| `number(x)` | Converts to number                | `number("42")` → `42`   |
| `string(x)` | Converts to string                | `string(42)` → `"42"`   |
| `type(x)`   | Returns the type name as a string | `type(42)` → `"number"` |

### number()
Converts a value to number. You will get an `none` value if conversion fails.

```apex
number("42")       // 42
number("3.14")     // 3.14
number(true)       // none (conversion fails)
number(false)      // none (conversion fails)
number("hello")    // none (conversion fails)
number([])         // none (conversion fails)
```

### string()
Converts any value to its string representation.

| Input            | Output               |
|------------------|----------------------|
| `42`             | `"42"`               |
| `3.14`           | `"3.14"`             |
| `none`           | `"none"`             |
| `true` / `false` | `"true"` / `"false"` |
| `[]`             | `"[]"`               |

```apex
string(42)      // "42"
string(3.14)    // "3.14"
string(none)    // "none"
string(true)    // "true"
string(false)   // "false"
string([])      // "[]"
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

| Operator | Name               | Example  |
|----------|--------------------|----------|
| `+`      | Addition           | `5 + 3`  |
| `-`      | Subtraction        | `10 - 4` |
| `*`      | Multiplication     | `7 * 6`  |
| `/`      | Division           | `15 / 4` |
| `%`      | Modulo (remainder) | `15 % 4` |

Arithmetic only works with the numbers data type, you cannot add number with string, string with boolean, etc. When you perform an arithmetic operation between a whole and a decimal, the result also becomes a decimal.

## 2.2 Comparison Operators
Comparison operators compare two values and give you a boolean result: either `true` or `false`. They can return only a Boolean value.

| Operator | Name                  | Example  |
|----------|-----------------------|----------|
| `==`     | Equal to              | `5 == 5` |
| `!=`     | Not equal to          | `5 != 3` |
| `<`      | Less than             | `3 < 5`  |
| `>`      | Greater than          | `5 > 3`  |
| `<=`     | Less than or equal    | `3 <= 3` |
| `>=`     | Greater than or equal | `5 >= 5` |

Comparison operators `<`, `>`, `<=`, `>=` work only with numbers. Using them with strings, booleans, tables, or none will result in a parse-time error. To check equality or inequality of any type, use `==` and `!=`.

## 2.3 Logical Operators
Logical operators combine boolean values (`true` or `false`) to create more complex conditions.

| Operator |          What It Does          |         Example        |
|----------|--------------------------------|------------------------|
| `and`    | Both sides must be true        | `(5 < 10) and (2 > 1)` |
| `or`     | At least one side must be true | `(2 > 1) or (2 < 1)`   |
| `not`    | Reverses the value             | `not true`             |

> Logical operators `and` & `or` requires explicit boolean condition. `not` also requires boolean variable.

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

| Statement | When It Runs                                                  |
|-----------|---------------------------------------------------------------|
| `if`      | Condition is `true`                                           |
| `else if` | Previous conditions were `false` AND this condition is `true` |
| `else`    | All previous conditions were `false`                          |

### Blocks and Indentation
The body of an `if`/`else` is a **block** — code that executes conditionally. Blocks are defined by **4-space indentation**.

```apex
import os

x = 5

if x < 10
    x = 42

os.output(x)  // 42 — visible
```

**No new scope is created.** Variables declared inside an `if` block remain accessible outside:

### Explicit Conditions Required
In Apex, conditions must be **explicitly boolean**. You cannot use variables directly as conditions (no "truthy" or "falsy" values). 

**This does NOT work:**
```apex
import os

x = 10

if x      // ERROR: If requires a comparison, got number
    os.output("Hello")
```

**You MUST write:**
```apex
import os

x = 10

if x > 5  // correct because comparison returns boolean
    os.output("Hello")
```

Always use comparison operators (`==`, `!=`, `<`, `>`, etc.) to create boolean values, then combine them with logical operators (`and`, `or`, `not`) if needed.

## 3.1 If Statement
```apex
import os

user = none

if user == none
    os.output("No user found")
```

## 3.2 Else-If Statement
```apex
import os

score = 85

if score >= 90
    os.output("Grade: A")
else if score >= 80
    os.output("Grade: B")
```

Apex checks conditions in order from top to bottom. As soon as one condition is `true`, it runs that block and skips the rest. The remaining `else if` blocks are never checked.

## 3.3 Else Statement
`else` catches everything that wasn't caught by `if` or `else if`. It runs when no other condition was `true`.

```apex
import os

score = 55

if score >= 60
    os.output("You passed!")
else
    os.output("You failed.")
```

### 3.4 Ternary Operator
The ternary operator is a shorthand form for simple conditional expressions. It's not a replacement for `if-else`, but a handy tool for cases when you need to assign a value to a variable based on a simple check.

```apex
import os

temperature = 25

weather = "hot" if temperature > 20 else "cold"

os.output(weather)  // hot
```

The ternary operator can only be used for short conditions. If the selection logic requires 2 or more checks, you must use regular `if-else if-else` blocks — 2 or more checks are not allowed in a ternary statement.

# 4. Match

`match` is how you compare one value against a list of fixed options. It's cleaner than a long chain of `if`/`else if` when every branch checks the **same value** against a **constant**.

| Statement | When It Runs |
|-----------|--------------|
| `case <constant>` | The subject equals that constant |
| `case` (no value) | No other case matched — the default |

Think of it as a specialized `if` that only answers one question: *"does this value equal one of these constants?"*

- **Subject** must be `number`, `string`, `boolean`, or `none`.
- **Patterns** must be constants of the same kind: number literal, string literal, `true`/`false`, `none`, or a negative number.
- Tables and functions **cannot** be used as subjects or patterns.
- The **default** case has no value and must be last.
- Only **one** default case is allowed.
- Cases are checked **top to bottom**; the first match wins.
- `match` does **not** create a new scope.
- If nothing matches and there is no default, execution continues after the `match`.

## 4.1 Match Statement
The `match` keyword is followed by the value you want to check — the **subject**. Then an indented block of `case` branches.

```apex
import os

status = 404

match status
    case 200
        os.output("OK")
    case 404
        os.output("Not Found")
    case 500
        os.output("Server Error")
```

Apex checks the cases from top to bottom. As soon as one matches, it runs that branch and skips the rest — exactly like `if`/`else if`.

Blocks use the same **4-space indentation** rule as `if` and `for`. No new scope is created, so variables declared inside a case stay visible after the `match`.

```apex
import os

code = 200

match code
    case 200
        message = "OK"

os.output(message)  // "OK" — visible outside
```

## 4.2 Case Patterns
A case pattern must be a **constant**: a number, a string, a boolean, or `none`. Negative numbers are allowed.

```apex
import os

grade = "B"

match grade
    case "A"
        os.output("Excellent")
    case "B"
        os.output("Good")
    case "C"
        os.output("Average")
```

The subject must be a `number`, `string`, `boolean`, or `none` — not a table or function. Apex also warns you when a pattern can never match the subject.

## 4.3 Default Case
A `case` with no value is the **default**. It runs when no other case matched.

```apex
import os

command = "quit"

match command
    case "start"
        os.output("Starting...")
    case "stop"
        os.output("Stopping...")
    case
        os.output("Unknown command")
```

The default case must be the **last** one. Putting anything after it is an error.

# 5. For Loops
Apex gives you the `for` loop with three different syntaxes to handle all these situations.

| Syntax                | When to Use                    | Example          |
|-----------------------|--------------------------------|------------------|
| `for r = start, end`  | You know the exact range       | `for r = 1, 5`   |
| `for v in table`      | Iterate over table items       | `for v in table` |
| `for c == true`       | Repeat while condition is true | `for c == true`  |

### Blocks and Scoping
Like `if`, loops use **4-space indentation** to define their body. Unlike `if`, **loops create a new scope.** The loop variable and any variables declared inside the loop are local to it:

```apex
import os

for i = 1, 3
    temp = i * 2     // local to loop
    os.output(temp)  // works

os.output(temp)      // ERROR — not defined
os.output(i)         // ERROR — not defined
```

## 5.1 For Counter
The counter creates a numeric loop. You specify a variable, a starting number, and an ending number. The loop runs once for each number in that range, including the end value.

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

## 5.2 For Table Iteration
You can iterate over any table using `for value in table`:

```apex
import os

fruits = ["apple", "banana", "cherry"]

for fruit in fruits
    os.output(fruit)
```

## 5.3 For Condition
When you don't know how many times you need to repeat, use a condition loop. As long as the condition is `true`, the loop keeps running.

```apex
import os

counter = 1

for counter <= 5
    os.output(counter)
    counter = counter + 1
```

## 5.4 Break
`break` exits the loop immediately — same as in `while`.

```apex
import os

for i = 1, 10
    if i == 5
        break
    os.output(i)
```

## 5.5 Continue
`continue` skips the rest of the current iteration and moves to the next number.

```apex
import os

for i = 1, 5
    if i == 3
        continue
    os.output(i)
```

# 6. Functions
### Blocks and Scope
Function bodies are defined by **4-space indentation**. Functions create a new **scope**: variables declared inside, including parameters, are local. The function can read outer variables but cannot be written to from the outside.

```apex
import os

x = "outer"

function test()
    y = "inner"
    os.output(x)  // OK — reads outer variable
    return none

test()
os.output(y)      // ERROR — y is not defined here
```

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

## 6.3 Return Value
Return value is what the function sends back after it finishes. You use the `return` keyword.

```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)  // Prints: 8
```

Functions without `return` return `none` automatically. Once `return` happens, the function exits. Nothing after it runs.

### 6.4 Call
Using a function is called calling it. You write the function name followed by parentheses.

```apex
// call a function with no parameters
say_hello()
// call with parameters
greet("Alice")
// store the return value
total = add(10, 5)
```

# 7. Imports
Imports give you the ability to use code from other files. Every import path is relative to the main file — the file you run with `apex file.apex`.

## 7.1 Importing an Entire File
To import everything from a file in the same folder:

```apex
import os
import database.apex

// use items with the filename as a prefix
database.connect()
os.output(database["APP_NAME"])
```

For user files, you must add `.apex` at the end.

## 7.2 Importing from Sub-folders
Use dots (`/`) to navigate into folders:

```
my_project/
├── main.apex
├── utils/
│   ├── math.apex
└── └── string.apex
```

```apex
import utils/math.apex
```

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
When you import a file, you get all globals from it:

- All functions
- All variables

## 7.4 Aliasing
When a module has a long or awkward name, you can give it a short alias with the `as` keyword. After that, use the alias instead of the full module name.

```apex
import utils/calculator.apex as calc

result = calc.add(2, 3)
calc.print_result(result)
```

- The alias must come after the file path.
- The alias must be a valid name (letters, digits, underscore; cannot start with a digit).
- An alias is **only** for user modules. Built-in modules (`os`, `sys`, `math`, etc.) cannot be aliased.

# Conclusion
## What's Next?
Now you know the basics of Apex! The best next step is to explore the built-in libraries—check out the **Library Reference**.

**Remember:**

> *Apex is designed to be simple, but it's powerful with libraries.*

**Happy coding in Apex!**