# Apex Library Reference
Apex comes with several built-in libraries. These are ready-to-use tools that solve common tasks: you don't need to write everything from scratch — just import the library you need and use it.

**Important:** All functions evaluate to `false` on error. Handle it with `if var == false` condition check.

## OS Library (os)
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
Reads the entire contents of a file and returns it as a string.

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
    os.exit(1)              // exit with error code
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

## Math Library (math)
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
Returns the square root of a number. You can't take the square root of a negative in real numbers.

```apex
import math
math.sqrt(25)      // 5
math.sqrt(2)       // 1.4142135623730951
math.sqrt(-1)      // false
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
Returns the logarithm of `x`. Without a base, uses the natural logarithm (base `e`). With a base, calculates the logarithm with that base.

```apex
import math
math.log(2.71828)       // ~1 (natural log of e)
math.log(100, 10)       // 2 (10² = 100)
math.log(8, 2)          // 3 (2³ = 8)
math.log(0)             // false
math.log(-5)            // false
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
Returns the arcsine of `x` in radians — the angle whose sine is `x`. Input must be between -1 and 1.

```apex
import math
math.asin(0)             // 0
math.asin(1)             // 1.5707963267948966 (π/2)
math.asin(2)             // false
```

### math.acos(x)
Returns the arccosine of `x` in radians — the angle whose cosine is `x`. Input must be between -1 and 1.

```apex
import math
math.acos(1)             // 0
math.acos(0)             // 1.5707963267948966 (π/2)
math.acos(2)             // false
```

### math.atan(x)
Returns the arctangent of `x` in radians — the angle whose tangent is `x`.

```apex
import math
math.atan(0)             // 0
math.atan(1)             // 0.7853981633974483 (π/4)
```

Here's the String Library section for the manual:

## String Library (string)
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

## Table Library (table)
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