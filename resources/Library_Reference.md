# Apex Library Reference
Apex comes with several built-in libraries. These are ready-to-use tools that solve common tasks: you don't need to write everything from scratch — just import the library you need and use it.

**Important:** Most functions return `none` on error. However, some functions may return `false` as a valid value (e.g., when a table contains `false`).

## OS Library (os)
The OS library lets you interact with the operating system, manage processes, and handle standard I/O. Import it with `import os`.

### os.output(value)
Prints a value to the terminal followed by a newline. Always returns `none`.

```apex
import os
os.output("Hello, Friend!")     // Hello, Friend!
```

### os.input(prompt)
Waits for the user to type something and press Enter. Returns what they typed as a string, or an empty string. You can provide an optional prompt message.

```apex
import os
name = os.input("What is your name? ")
os.output("Hello, {name}")
```

### os.wait(seconds)
Pauses the program for the given number of seconds. You can use decimals for fractions of a second. Negative values are treated as `0`. Always returns `none`.

```apex
import os

os.output("Starting...")
os.wait(0.5)
os.output("Done waiting")
```

### os.exit(code)
Exits the program immediately. The `code` is optional — `0` means success, other numbers mean an error. If no code is given, uses `0`. Never returns.

```apex
import os

if os.exists("my_file.txt") != true
    os.exit(1)
```

### os.execute(command)
Runs a system command as if you typed it in the terminal. Returns the command's exit code as a number, or `none` if the command could not be executed.

```apex
import os

command = os.execute("echo Hello, Friend!")

if command != none
    os.output("Command exited with code: {command}")
```

### os.terminate(pid)
Terminates the process with the given PID. Returns `true` on success, `false` on failure.

```apex
import os

if os.terminate(123456) == false
    os.output("Could not terminate process")
else
    os.output("Process terminated successfully")
```

### os.current_folder()
Returns the current directory as a string. Returns `none` on failure.

```apex
import os

current_folder = os.current_folder()

if current_folder != none
    os.output("Running from: {current_folder}")
```

### os.change_folder(path)
Changes the current directory. Returns `true` on success, `false` on failure.

```apex
import os

if os.change_folder("/home") == false
    os.output("Could not change directory")
else
    os.output("Directory changed successfully")
```

### os.read(filename)
Reads the entire contents of a file and returns it as a string. Returns `none` if the file cannot be read.

```apex
import os

content = os.read("my_file.txt")

if content == none
    os.output("Could not read the file")
else
    os.output(content)
```

### os.write(filename, content)
Writes content to a file. Creates the file if it doesn't exist, overwrites it if it does. Returns `true` on success, `false` on failure.

```apex
import os

if os.write("my_file.txt", "Hello, Friend!") == false
    os.output("Could not write to the file")
else
    os.output("File saved successfully")
```

### os.append(filename, content)
Appends content to the end of a file. Creates the file if it doesn't exist. Returns `true` on success, `false` on failure.

```apex
import os

if os.append("my_file.txt", "Second line") == false
    os.output("Could not append to the file")
else
    os.output("Second line appended")
```

### os.exists(path)
Returns `true` if the file or folder at `path` exists, `false` otherwise.

```apex
import os

if os.exists("my_file.txt") == false
    os.output("File not found")
else
    os.output("File found")
```

### os.is_file(path)
Returns `true` if the path points to a file specifically (not a folder). Returns `false` otherwise.

```apex
import os

if os.is_file("my_file.txt") == false
    os.output("Not a file or doesn't exist")
else
    os.output("It's a file")
```

### os.is_folder(path)
Returns `true` if the path points to a folder specifically. Returns `false` otherwise.

```apex
import os

if os.is_folder("my_folder") == false
    os.output("Not a folder or doesn't exist")
else
    os.output("It's a folder")
```

### os.size(path)
Returns the size of a file or directory in bytes. For directories, it calculates the total size recursively. Returns a number on success, `none` if the path doesn't exist.

```apex
import os

size = os.size("my_file.mp4")

if size == none
    os.output("Could not get size")
else
    os.output("Size: {size} bytes")
```

### os.rename(old_name, new_name)
Renames a file or directory. Returns `true` on success, `false` on failure.

```apex
import os

if os.rename("old.txt", "new.txt") == false
    os.output("Could not rename")
else
    os.output("Renamed successfully")
```

### os.move(source, destination)
Moves a file or directory to a new location. Returns `true` on success, `false` on failure.

```apex
import os

if os.move("my_file.txt", "backup/my_file.txt") == false
    os.output("Could not move")
else
    os.output("Moved successfully")
```

### os.copy(source, destination)
Copies a file or recursively copies a directory with all its contents. Returns `true` on success, `false` on failure.

```apex
import os

if os.copy("original.txt", "copy.txt") == false
    os.output("Could not copy")
else
    os.output("Copied successfully")
```

### os.create_file(filename)
Creates an empty file. Returns `true` on success, `false` on failure.

```apex
import os

if os.create_file("my_file.txt") == false
    os.output("Could not create the file")
else
    os.output("File created successfully")
```

### os.create_folder(path)
Creates a new folder. Returns `true` on success, `false` if the folder already exists or can't be created.

```apex
import os

if os.create_folder("my_folder") == false
    os.output("Folder already exists or can't be created")
else
    os.output("Folder created successfully")
```

### os.delete(path)
Deletes a file or an empty folder. Returns `true` if deleted, `false` if the path doesn't exist, is a non-empty folder, or can't be deleted.

```apex
import os

if os.delete("my_file.txt") == false
    os.output("Could not delete")
else
    os.output("Deleted successfully")
```

### os.list_folder(path)
Returns a table of names — all files and folders inside the given folder. If no path is given, lists the current folder. Returns `none` on failure.

```apex
import os

items = os.list_folder(".")

if items == none
    os.output("Could not list directory contents")
else
    os.output("Contents:")
    for item = items
        os.output(item)
```

### os.parent_folder(path)
Returns the parent directory of the given path. For root paths like `/` or `C:\`, returns the root itself. If no directory separator is found in the path, returns `"."`. Returns `none` on failure or if no path is provided.

```apex
import os

parent = os.parent_folder("/home/user/my_folder")

if parent != none
    os.output("Parent folder: {parent}")
```

### os.access(path, mode)
Changes file permissions. The `mode` is a number (e.g., `755` for rwxr-xr-x on Unix). Returns `true` on success, `false` on failure.

```apex
import os

if os.access("script.sh", 755) == false
    os.output("Could not change permissions")
else
    os.output("Permissions changed successfully")
```

### os.args()
Returns a table of command line arguments passed to the script. Arguments are 1-indexed: `args[1]` is the first user argument, `args[2]` is the second, and so on. The interpreter name and script filename are excluded. Returns an empty table when no arguments are provided (e.g., in REPL or stdin mode).

```apex
import os

args = os.args()

if args[1] == none
    os.output("No arguments provided")
else
    os.output("Arguments:")
    i = 1
    for arg = args
        os.output("  [{i}] = {arg}")
        i = i + 1
```

## System Library (sys)
The System library provides static or rarely changing system information. Import it with `import sys`.

### sys.time()
Returns the current time as a number — seconds since January 1, 1970 (with microsecond precision). Always succeeds.

```apex
import os
import sys

start = sys.time()

for i = 1, 100000
    i = i + 1

end = sys.time()

os.output("Took {end - start} seconds")
```

### sys.datetime()
Returns the current UTC date and time as a table. All values are numbers. The table contains the following keys:

- `year` — The current year (e.g., 2026)
- `month` — The month as a number (1-12)
- `week` — The day of the week as a number (0 = Sunday, 6 = Saturday)
- `day` — The day of the month (1-31)
- `hour` — The hour in 24-hour format (0-23)
- `minute` — The minute (0-59)
- `second` — The second (0-59)
- `millisecond` — The millisecond (0-999)

```apex
import os
import sys

now = sys.datetime()

os.output("Year: {now['year']}")
os.output("Month: {now['month']}")
os.output("Day: {now['day']}")
os.output("Hour: {now['hour']}")
os.output("Minute: {now['minute']}")
```

### sys.platform()
Returns a string identifying your operating system, such as `"Windows"`, `"macOS"`, `"iOS"`, `"tvOS"`, `"watchOS"`, `"Android"`, `"Linux"`, `"FreeBSD"`, `"OpenBSD"`, `"NetBSD"`, `"QNX"`, or `"Unix"`. Returns `none` if the platform cannot be detected.

```apex
import os
import sys

system = sys.platform()

if system != none
    os.output("You're running on {system}")
```

### sys.architecture()
Returns a string identifying the system's processor architecture. Possible return values:

- `"x86-64"` — 64-bit x86
- `"arm64"` — 64-bit ARM
- `"x86"` — 32-bit x86
- `"arm"` — 32-bit ARM

Returns `none` if the architecture cannot be detected.

```apex
import os
import sys

arch = sys.architecture()

if arch != none
    os.output("System Architecture: {arch}")
```

### sys.host()
Returns the system's hostname as a string. Returns `none` on failure.

```apex
import os
import sys

host = sys.host()

if host != none
    os.output("Hostname: {host}")
```

### sys.user()
Returns the current user's login name as a string. Returns `none` if it can't be determined.

```apex
import os
import sys

user = sys.user()

if user != none
    os.output("Logged in as: {user}")
```

### sys.home()
Returns the current user's home directory path as a string. Returns `none` if it can't be determined.

```apex
import os
import sys

home = sys.home()

if home != none
    os.output("Home folder: {home}")
```

### sys.is_terminal(fd)
Checks if the file descriptor goes to a terminal. By default checks stdout (fd 1). Pass a number to check a different file descriptor: `0` for stdin, `2` for stderr. Returns `true` if the output goes to a terminal, `false` if it's redirected to a file or pipe. Always succeeds.

```apex
import os
import sys

if sys.is_terminal() == true
    os.output("Output is a terminal")
else
    os.output("Output is not a terminal")
```

### sys.apex_version()
Returns the current Apex interpreter version as a string. Always succeeds.

```apex
import os
import sys

os.output("Apex Version: {sys.apex_version()}")
```

### sys.executable()
Returns the full path to the currently running Apex executable. Returns `none` on failure.

```apex
import os
import sys

path = sys.executable()

if path != none
    os.output("Running from: {path}")
```

### sys.disk(path)
Returns a table with disk usage information for the volume containing the given path. If no path is provided, uses the current directory. The table contains:
- `total` — Total size in MB
- `used` — Used space in MB
- `free` — Free space in MB

Returns a table on success, `none` on failure.

```apex
import os
import sys

info = sys.disk()

if info != none
    os.output("Total: {info['total'] / 1024} GB")
    os.output("Free: {info['free'] / 1024} GB")
```

### sys.temp()
Returns the path to the system's temporary directory. On Unix-like systems, checks the `TMPDIR`, `TMP`, and `TEMP` environment variables, falling back to `/tmp`. Returns `none` on failure.

```apex
import os
import sys

temp_dir = sys.temp()

if temp_dir != none
    os.output("Temporary directory: {temp_dir}")
```

### sys.environment()
Returns a table containing all environment variables as key-value pairs. Always returns a table (empty if no variables).

```apex
import os
import sys

env_vars = sys.environment()

os.output("HOME = {env_vars['HOME']}")
```

### sys.process_id()
Returns the current process ID as a number. Always succeeds.

```apex
import os
import sys

pid = sys.process_id()

os.output("Process ID: {pid}")
```

## Math Library (math)
The Math library provides mathematical functions beyond basic arithmetic. Import it with `import math`.

### math.pi()
Returns the mathematical constant π (pi), approximately 3.14159. Pi represents the ratio of a circle's circumference to its diameter.

```apex
import math
math.pi()  // 3.141592653589793
```

### math.e
Returns Euler's number (e), approximately 2.71828. This constant is the base of natural logarithms and appears in growth rates, compound interest, and many natural processes.

```apex
import math
math.e()  // 2.718281828459045
```

### math.inf
Returns positive infinity. This represents a value larger than any finite number. Useful for comparisons and algorithm boundaries.

```apex
import math
math.inf()  // inf
```

### math.abs(x)
Returns the absolute value of a number — how far it is from zero, ignoring the sign. Returns `none` if the argument is not a number.

```apex
import math
math.abs(5)      // 5
math.abs(-5)     // 5
math.abs(-3.14)  // 3.14
```

### math.round_down(x)
Rounds a number down to the nearest whole number. Returns `none` if the argument is not a number.

```apex
import math
math.round_down(3.7)   // 3
math.round_down(3.1)   // 3
math.round_down(-2.3)  // -3 (goes down, so more negative)
```

### math.round_up(x)
Rounds a number up to the nearest whole number. Returns `none` if the argument is not a number.

```apex
import math
math.round_up(3.1)   // 4
math.round_up(3.7)   // 4
math.round_up(-2.3)  // -2 (goes up toward zero)
```

### math.round(x, digits)
Rounds a number to the nearest whole number, or to a specific number of decimal places. The `digits` parameter is optional — without it, rounds to a whole number. At exactly .5, rounds up. Returns `none` on error.

```apex
import math
math.round(3.4)         // 3
math.round(3.6)         // 4
math.round(3.5)         // 4 (rounds up at .5)
math.round(3.14159, 2)  // 3.14
math.round(3.14159, 3)  // 3.142
```

### math.drop_decimal(x)
Truncates a number by removing the decimal part. Returns `none` if the argument is not a number.

```apex
import math
math.drop_decimal(3.7)   // 3
math.drop_decimal(-3.7)  // -3
math.drop_decimal(0.9)   // 0
```

### math.sqrt(x)
Returns the square root of a number. Returns `nan` for negative numbers. Returns `none` if the argument is not a number. You can check the result with `math.is_nan()`.

```apex
import math
math.sqrt(25)  // 5
math.sqrt(2)   // 1.4142135623730951
math.sqrt(-1)  // nan
```

### math.power(base, exponent)
Raises a number to a power. Returns `base` raised to the `exponent`. Returns `none` on error.

```apex
import math
math.power(2, 3)    // 8
math.power(4, 0.5)  // 2 (same as square root)
math.power(10, -1)  // 0.1
```

### math.exponent(x)
Returns `e` raised to the power of `x`. Returns `none` if the argument is not a number.

```apex
import math
math.exponent(1)  // 2.718281828459045
math.exponent(0)  // 1
math.exponent(2)  // 7.38905609893065
```

### math.hypotenuse(x, y)
Returns the hypotenuse of a right triangle given the lengths of the two legs. Returns `none` on error.

```apex
import math
math.hypotenuse(3, 4)   // 5
math.hypotenuse(5, 12)  // 13
math.hypotenuse(1, 1)   // 1.4142135623730951
```

### math.log(x, base)
Returns the logarithm of `x`. Without a base, uses the natural logarithm (base `e`). With a base, calculates the logarithm with that base. Returns `nan` for zero or negative inputs. Returns `none` on error.

```apex
import math
math.log(2.718281828459045)  // ~1 (natural log of e)
math.log(100, 10)            // 2 (10² = 100)
math.log(8, 2)               // 3 (2³ = 8)
math.log(0)                  // nan
math.log(-5)                 // nan
```

### math.sin(x)
Returns the sine of `x` radians.

```apex
import math
math.sin(0)              // 0
math.sin(math.pi() / 2)  // 1
math.sin(math.pi())      // ~0
```

### math.cos(x)
Returns the cosine of `x` radians.

```apex
import math
math.cos(0)              // 1
math.cos(math.pi() / 2)  // ~0
math.cos(math.pi())      // -1
```

### math.tan(x)
Returns the tangent of `x` radians.

```apex
import math
math.tan(0)              // 0
math.tan(math.pi() / 4)  // ~1
```

### math.asin(x)
Returns the arcsine of `x` in radians. Input must be between -1 and 1. Returns `nan` for values outside this range. Returns `none` on error.

```apex
import math
math.asin(0)  // 0
math.asin(1)  // 1.5707963267948966 (π/2)
math.asin(2)  // nan
```

### math.acos(x)
Returns the arccosine of `x` in radians. Input must be between -1 and 1. Returns `nan` for values outside this range. Returns `none` on error.

```apex
import math
math.acos(1)  // 0
math.acos(0)  // 1.5707963267948966 (π/2)
math.acos(2)  // nan
```

### math.atan(x)
Returns the arctangent of `x` in radians. Returns `none` on error.

```apex
import math
math.atan(0)  // 0
math.atan(1)  // 0.7853981633974483 (π/4)
```

### math.atan2(y, x)
Returns the arctangent of `y/x` in radians, using the signs of both arguments to determine the correct quadrant. Returns `none` on error.

```apex
import math
math.atan2(1, 1)   // 0.7853981633974483 (π/4)
math.atan2(1, 0)   // 1.5707963267948966 (π/2)
math.atan2(-1, 0)  // -1.5707963267948966 (-π/2)
```

### math.radians(degrees)
Converts degrees to radians. Returns `none` on error.

```apex
import math
math.radians(180)  // 3.141592653589793 (π)
math.radians(90)   // 1.5707963267948966 (π/2)
math.radians(360)  // 6.283185307179586 (2π)
```

### math.degrees(radians)
Converts radians to degrees. Returns `none` on error.

```apex
import math
math.degrees(math.pi())      // 180
math.degrees(math.pi() / 2)  // 90
math.degrees(1)              // 57.29577951308232
```

### math.gcd(a, b)
Returns the greatest common divisor of two numbers. Both arguments are converted to positive integers before calculation. Returns `none` on error.

```apex
import math
math.gcd(12, 8)   // 4
math.gcd(17, 5)   // 1
math.gcd(48, 18)  // 6
```

### math.factorial(n)
Returns the factorial of a non-negative integer `n` (the product of all positive integers from 1 to `n`). The maximum allowed input is 170 — larger values will return `inf`. Returns `none` on error.

```apex
import math
math.factorial(0)    // 1
math.factorial(1)    // 1
math.factorial(5)    // 120
math.factorial(10)   // 3628800
math.factorial(-3)   // none
math.factorial(2.5)  // none
math.factorial(171)  // inf (too large)
```

### math.is_nan(x)
Returns `true` if the value is NaN (Not a Number), `false` otherwise. NaN typically results from operations like `sqrt(-1)` or `0/0`. Returns `false` if the argument is not a number.

```apex
import math
math.is_nan(math.sqrt(-1))  // true
math.is_nan(0)              // false
math.is_nan(42)             // false
math.is_nan("42")           // false
```

### math.is_inf(x)
Returns `true` if the value is positive or negative infinity, `false` otherwise. Infinity often results from division by zero or overflowing calculations. Returns `false` if the argument is not a number.

```apex
import math
math.is_inf(math.inf())   // true
math.is_inf(-math.inf())  // true
math.is_inf(1000)         // false
math.is_inf(0)            // false
math.is_inf("0")          // false
```

## String Library (string)
The String library helps you work with text — measure length, change case, find words, split and combine strings, and more. Import it with `import string`.

### string.length(s)
Returns the number of characters in a string. Spaces and punctuation count as characters too. Returns `none` if the argument is not a string.

```apex
import os
import string

result = string.length("Hello, Friend!")

if result != none
    os.output(result)  // 14
```

### string.lower(s)
Converts every character in the string to lowercase. Returns `none` if the argument is not a string.

```apex
import os
import string
os.output(string.lower("HELLO"))  // "hello"
```

### string.upper(s)
Converts every character in the string to uppercase. Returns `none` if the argument is not a string.

```apex
import os
import string
os.output(string.upper("hello"))     // "HELLO"
```

### string.slice(s, start, end)
Extracts a portion of a string — from `start` to `end`, but not including `end`. Positions start counting from `1`. Returns `none` on error.

```apex
import os
import string

text = "Hello, World"
result = string.slice(text, 1, 5)

if result != none
    os.output(result)  // "Hello"
```

If `start` is negative, it's treated as `1`. If `end` is larger than the string length, it stops at the end.

```apex
import os
import string

sub = string.slice("Apex", 2, 10)

os.output(sub)  // "pex" (end is bigger than string, stops at end)
```

### string.split(s, separator)
Splits a string into a table of substrings. The separator is the character (or characters) where the split happens. If no separator is given, splits on whitespace. Returns `none` if the argument is not a string.

```apex
import os
import string

result = string.split("apple,banana,orange", ",")

if result != none
    os.output(result[1])  // "apple"
```

### string.join(parts, separator)
Does the opposite of `split` — takes a table of strings and joins them into one string with a separator between each. Returns `none` if the first argument is not a table.

```apex
import os
import string

result = string.join(["Hello", "World"], " ")

if result != none
    os.output(result)        // "Hello World"
```

### string.trim(s)
Removes whitespace (spaces, tabs, newlines) from the beginning and end of a string. The middle spaces are left alone. Returns `none` if the argument is not a string.

```apex
import os
import string
os.output(string.trim("\n  text \n"))        // "text"
```

### string.find(s, search)
Searches for `search` inside `s` and returns the position of the first match. Returns `-1` if not found. Position starts from `1`. Returns `none` on error.

```apex
import os
import string

result = string.find("Hello World", "World")

if result != none
    os.output(result)  // 7
```

### string.replace(s, old, new)
Replaces **all** occurrences of `old` with `new` in the string. If `old` isn't found, returns the original string unchanged. Returns `none` on error.

```apex
import os
import string

result = string.replace("Hello World Hello", "Hello", "Apex")

if result != none
    os.output(result)  // "Apex World Apex"
```

## Table Library (table)
The Table library provides functions for working with tables. Import it with `import table`.

### table.size(t)
Returns the number of items in the table. Always returns a number (0 if empty).

```apex
import os
import table

user = ["name" = "Alice", "age" = 30]
os.output(table.size(user))    // 2

colors = ["red", "green", "blue"]
os.output(table.size(colors))  // 3

empty = []
os.output(table.size(empty))   // 0
```

### table.has(t, key)
Returns `true` if the table has the specified key. Returns `false` if the key does not exist. Returns `none` if the first argument is not a table. Keys are type-sensitive. The number `1` and the string `"1"` are different keys.

```apex
import os
import table

user = ["name" = "Alice", "age" = 30]
result = table.has(user, "name")

if result == none
    os.output("Invalid key type")
else if result == true
    os.output("Key exists")
else
    os.output("Key does not exist")
```

### table.remove(t, key)
Removes an item from a table by key. Returns `true` if the key existed and was removed, `false` if the key did not exist. Returns `none` if the first argument is not a table. Keys are type-sensitive. The number `1` and the string `"1"` are different keys.

```apex
import os
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
result = table.remove(user, "age")

if result != none
    os.output("Removed: {result}")  // true

colors = ["red", "green", "blue"]
result = table.remove(colors, 2)

if result != none
    os.output("Removed: {result}")  // true
```

### table.keys(t)
Returns a table of all keys in the table. Keys are sorted: numbers first in ascending order, then strings lexicographically (alphabetically). Always returns a table (empty if source is empty).

```apex
import os
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
os.output(table.keys(user))  // ["active", "age", "name"]

colors = ["red", "green", "blue"]
os.output(table.keys(colors))  // ["1", "2", "3"]
```

### table.values(t)
Returns a table of all values, ordered by their keys. Keys sorted: numbers ascending, then strings lexicographically. Always returns a table.

```apex
import os
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
os.output(table.values(user))  // [true, 30, "Alice"]

colors = ["red", "green", "blue"]
os.output(table.values(colors))  // ["red", "green", "blue"]
```

### table.clear(t)
Removes all items from the table. Returns `true` on success, `none` on failure.

```apex
import os
import table

user = ["name" = "Alice", "age" = 30]
table.clear(user)  // user is now []
```

### table.copy(t)
Returns a shallow copy of the table. Changes to the copy don't affect the original. Always returns a table.

**Important**: This function may return `false` as a valid value if the source table contains `false`. Always check for `none` to detect errors (though this function never returns `none`).

```apex
import os
import table

original = ["name" = "Alice", "age" = 30]
duplicate = table.copy(original)

if duplicate != none
    os.output(duplicate['name'] = "Bob")
    os.output(original['name'])  // still "Alice"
```

### table.merge(t1, t2)
Merges two tables into a new table. If keys conflict, values from the second table overwrite the first. Returns `none` if the second argument is not a table.

**Important**: This function may return `false` as a valid value if the merged table contains `false`. Always check for `none` to detect errors.

```apex
import os
import table

t1 = ["name" = "Alice", "age" = 30]
t2 = ["city" = "Dubai", "age" = 31]
merged = table.merge(t1, t2)

if merged != none
    os.output(merged)  // ["age" = 31, "city" = "Dubai", "name" = "Alice"]
```

## FFI Library (ffi)
The FFI library lets you call functions from shared libraries (.so on Linux, .dll on Windows). You can load libraries, call C functions, and manage memory. Import it with `import ffi`.

### ffi.open(path)
Loads a shared library from the given path and returns a table representing the library. The table contains `_handle` (internal numeric handle) and `path` (the library path). Returns `none` if the library cannot be loaded.

If the path does not contain a slash or backslash, "./" is prepended to search in the current directory.

```apex
import os
import ffi

lib = ffi.open("libc.so.6")

if lib == none
    os.output("Could not load library")
else
    os.output("Library loaded: {lib['path']}")
```

### ffi.call(lib_table, func_name, ...)
Calls a function from a loaded library. The first argument is the library table returned by `ffi.open()`, the second is the function name as a string, followed by optional arguments.

Functions are assumed to return `long` and accept up to 4 `long` arguments. Arguments are converted to numbers before passing. Returns the result as a number, or `none` on failure.

**Important**: This function may return `0` as a valid result. Always check for `none` to detect errors.

```apex
import os
import ffi

lib = ffi.open("libc.so.6")

if lib == none
    os.output("Could not load libc")
else
    pid = ffi.call(lib, "getpid")
    
    if pid == none
        os.output("Failed to call getpid")
    else
        os.output("Process ID: {pid}")
```

### ffi.errno()
Returns the current value of `errno` as a number. Always succeeds.

```apex
import os
import ffi

lib = ffi.open("nonexistent.so")

if lib == none
    err = ffi.errno()
    os.output("Error code: {err}")
```

### ffi.strerror(code)
Returns a human-readable error message for the given error code. If no code is provided, uses the current `errno`. Always returns a string.

```apex
import os
import ffi

lib = ffi.open("nonexistent.so")

if lib == none
    err = ffi.errno()
    msg = ffi.strerror(err)
    os.output("Error: {msg}")
```

### ffi.malloc(size)
Allocates `size` bytes of memory and returns the pointer as a number. Returns `none` if allocation fails.

```apex
import os
import ffi

ptr = ffi.malloc(1024)

if ptr == none
    os.output("Memory allocation failed")
else
    os.output("Allocated memory at: {ptr}")
    ffi.free(ptr)
```

### ffi.free(ptr)
Frees memory previously allocated by `ffi.malloc()`. Takes the pointer number as an argument. Does nothing if the pointer is `0` (NULL). Returns `none` if the argument is invalid, otherwise returns `true`.

```apex
import os
import ffi

ptr = ffi.malloc(512)

if ptr != none
    result = ffi.free(ptr)
    if result == true
        os.output("Memory freed")
    else
        os.output("Failed to free memory")
```

## Random Library (random)
The Random library provides functions for generating pseudo-random numbers and performing random operations on data structures. Import it with `import random`.

### random.seed(value)
Initializes the random number generator with a specific seed value. Using the same seed will produce the same sequence of random numbers, which is useful for reproducibility. If called without arguments, it seeds using the current system time. Always returns `none`.

```apex
import os
import random

random.seed(12345)
r1 = random.float()

random.seed(12345)
r2 = random.float()

if r1 == r2
    os.output("Seeds match!")
```

### random.float()
Returns a random floating-point number in the range `[0.0, 1.0)`. Always succeeds.

```apex
import os
import random

val = random.float()
os.output("Random float: {val}")
```

### random.integer(a, b)
Returns a random integer `N` such that `a <= N <= b`. If `a > b`, the bounds are swapped automatically. Returns `none` if arguments are not numbers.

```apex
import os
import random

dice = random.integer(1, 6)

if dice != none
    os.output("You rolled a {dice}")
```

### random.choice(seq)
Returns a random element from a non-empty table `seq`. Returns `none` if the table is empty or the argument is not a table.

**Important**: This function may return `false` as a valid value if the table contains `false`. Always check for `none` to detect errors.

```apex
import os
import random

colors = ["red", "green", "blue"]
pick = random.choice(colors)

if pick == none
    os.output("No elements in table")
else
    os.output("Selected color: {pick}")
```

### random.shuffle(seq)
Shuffles the elements of a table `seq` in place. The table must use sequential numeric keys (e.g., `[1, 2, 3]`). Returns `none` on success or error (check `none` for error detection).

```apex
import os
import random

deck = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
result = random.shuffle(deck)

if result == none
    os.output("Shuffled deck: {deck}")
else
    os.output("Failed to shuffle")
```

### random.sample(seq, k)
Returns a new table containing `k` unique elements chosen from the table `seq`. Used for random sampling without replacement. Returns `none` if `k` is larger than the size of `seq` or arguments are invalid.

```apex
import os
import random

pool = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
winners = random.sample(pool, 3)

if winners == none
    os.output("Sampling failed")
else
    os.output("Winners: {winners}")
```

### random.normal(mu, sigma)
Returns a random floating-point number from a Gaussian (normal) distribution with mean `mu` and standard deviation `sigma`. Returns `none` if arguments are not numbers.

```apex
import os
import random

height = random.normal(175, 10)

if height != none
    os.output("Simulated height: {height} cm")
```

### random.triangular(low, high, mode)
Returns a random floating-point number from a triangular distribution. `low` and `high` default to 0 and 1, while `mode` defaults to the midpoint between them. Returns `none` if arguments are not numbers.

```apex
import os
import random

val = random.triangular(0, 10, 5)

if val != none
    os.output("Triangular sample: {val}")
```

### random.expovariate(lambd)
Returns a random floating-point number from an exponential distribution with rate parameter `lambd`. Returns `none` if `lambd` is zero or not a number.

```apex
import os
import random

wait_time = random.expovariate(0.5)

if wait_time != none
    os.output("Expected wait: {wait_time} minutes")
```

### random.betavariate(alpha, beta)
Returns a random floating-point number from a Beta distribution with parameters `alpha` and `beta`. Both parameters must be greater than zero. Returns `none` otherwise.

```apex
import os
import random

probability = random.betavariate(2, 5)

if probability != none
    os.output("Beta sample: {probability}")
```

## JSON Library (json)
The JSON library provides encoding and decoding functions for JSON format. Import it with `import json`.

### json.encode(value)
Converts an Apex value (none, boolean, number, string, table) to a JSON string. Returns the JSON string, or `none` on failure. Tables are encoded as objects `{}` if they have named keys, or arrays `[]` if they only have sequential numeric keys.

```apex
import os
import json

data = ["name" = "Alice", "age" = 30]
json_str = json.encode(data)

if json_str == none
    os.output("JSON encoding failed")
else
    os.output(json_str)  // {"age": 30, "name": "Alice"}
```

### json.decode(json_string)
Parses a JSON string into an Apex value. Returns the parsed value (table, number, bool, string), or `none` on failure.

```apex
import os
import json

json_str = '\{"name": "Alice", "age": 30\}'
data = json.decode(json_str)

if data == none
    os.output("JSON parsing failed")
else
    os.output("Name: {data['name']}")  // Name: Alice
```

## XML Library (xml)
The XML library provides encoding and decoding functions for XML format. Import it with `import xml`.

### xml.encode(table)
Converts a table representing an XML structure to an XML string. The table should have a `__tag` key for the element name, `@key` keys for attributes, and `#text` for text content. Nested elements are stored with numeric keys. Returns the XML string, or `none` on failure.

```apex
import os
import xml

xml_data = [
    "__tag" = "root",
    "@id" = "1",
    1 = [
        "__tag" = "child",
        "#text" = "Hello"
    ]
]

xml_str = xml.encode(xml_data)

if xml_str == none
    os.output("XML encoding failed")
else
    os.output(xml_str)
```

### xml.decode(xml_string)
Parses an XML string into a table structure. Returns the root element as a table, or `none` on failure.

```apex
import os
import xml

xml_str = '<root id="1"><child>Hello</child></root>'
data = xml.decode(xml_str)

if data == none
    os.output("XML parsing failed")
else
    os.output("Tag: {data['__tag']}")
```

## CSV Library (csv)
The CSV library provides encoding and decoding functions for RFC 4180 compliant CSV format. Import it with `import csv`.

### csv.encode(table)
Converts a table of tables to a CSV string. The first row's keys are used as column headers. Returns the CSV string, or `none` on failure.

```apex
import os
import csv

data = [
    1 = ["name" = "Alice", "age" = 30],
    2 = ["name" = "Bob", "age" = 25]
]

csv_str = csv.encode(data)

if csv_str == none
    os.output("CSV encoding failed")
else
    os.output(csv_str)
```

### csv.decode(csv_string)
Parses an RFC 4180 compliant CSV string into a table of tables. The first row is used as column headers for all subsequent rows. Returns a table of rows, or `none` on failure.

```apex
import os
import csv

csv_str = "name,age\nAlice,30\nBob,25"
data = csv.decode(csv_str)

if data == none
    os.output("CSV parsing failed")
else
    os.output("First name: {data[1]['name']}")
```

## Base Library (base)
The Base library provides encoding and decoding functions for Base16, Base32, Base32Hex, Base64, and Base64URL formats. Import it with `import base`.

### base.encode_64(data)
Encodes a string to standard Base64. Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_64("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // SGVsbG8sIEZyaWVuZCE=
```

### base.decode_64(data)
Decodes a standard Base64 string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_64("SGVsbG8sIEZyaWVuZCE=")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_64url(data)
Encodes a string to URL-safe Base64. Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_64url("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded) // SGVsbG8sIEZyaWVuZCE
```

### base.decode_64url(data)
Decodes a URL-safe Base64 string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_64url("SGVsbG8sIEZyaWVuZCE")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_16(data)
Encodes a string to Base16 (hexadecimal). Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_16("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // 48656C6C6F2C20467269656E6421
```

### base.decode_16(data)
Decodes a Base16 (hexadecimal) string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_16("48656C6C6F2C20467269656E6421")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_32(data)
Encodes a string to standard Base32. Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_32("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // JBSWY3DPFQQEM4TJMVXGIII=
```

### base.decode_32(data)
Decodes a standard Base32 string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_32("JBSWY3DPFQQEM4TJMVXGIII=")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_32hex(data)
Encodes a string to Base32Hex (extended hex alphabet). Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_32hex("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // 91IMOR3F5GG4CSJ9CLN6888=
```

### base.decode_32hex(data)
Decodes a Base32Hex string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_32hex("91IMOR3F5GG4CSJ9CLN6888=")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_62(data)
Encodes a string to Base62 (alphanumeric characters 0-9, A-Z, a-z). Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_62("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // 80nEsxhzYSyNfj8Z0hl
```

### base.decode_62(data)
Decodes a Base62 string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_62("80nEsxhzYSyNfj8Z0hl")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

### base.encode_85(data)
Encodes a string to Ascii85 (also known as Base85). Returns the encoded string, or `none` on failure.

```apex
import os
import base

encoded = base.encode_85("Hello, Friend!")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // 87cURD_*##EbT*&A0C
```

### base.decode_85(data)
Decodes an Ascii85 string. Returns the decoded string, or `none` on failure.

```apex
import os
import base

decoded = base.decode_85("87cURD_*##EbT*&A0C")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello, Friend!
```

## Regex Library (regex)
The Regex library provides functions for working with regular expressions: searching, matching, replacing, and splitting text. Import it with `import regex`.

### regex.search(pattern, text, options)
Searches for the **first** match of a pattern in the text. Returns a table with information about the match, or an empty table if no match is found. The returned table contains:
- `start` — The starting position of the match (1-based index)
- `end` — The ending position of the match
- `match` — The matched string

Returns `none` on error.

```apex
import os
import table
import regex

result = regex.search("\\d+", "Order #12345")

if result == none
    os.output("Search failed")
else if table.size(result) > 0
    os.output("Found: '{result['match']}' at position {result['start']}-{result['end']}")
else
    os.output("No match found")
```

### regex.find_all(pattern, text, options)
Finds **all non-overlapping** matches of a pattern in the text. Returns a table of matched strings, indexed by numbers starting from `1`. Returns an empty table if no matches are found, or `none` on error.

```apex
import os
import regex

words = regex.find_all("\\w+", "Hello, World! 123")

if words == none
    os.output("Find failed")
else
    for word = words
        os.output(word)
```

### regex.replace(pattern, replacement, text, options)
Replaces **all** occurrences of the pattern in the text with the replacement string. Returns the resulting string. If no matches are found, the original text is returned unchanged. Returns `none` on error.

```apex
import os
import regex

text = "The quick brown fox jumps over the lazy dog"
result = regex.replace("\\s+", "-", text)

if result == none
    os.output("Substitution failed")
else
    os.output(result)
```

### regex.split(pattern, text, options)
Splits the text at each match of the pattern. Returns a table of substrings, indexed by numbers starting from `1`. Returns a table containing the original text if the pattern is not found, or `none` on error.

```apex
import os
import regex

parts = regex.split(",\\s*", "apple, banana, orange, grape")

if parts == none
    os.output("Split failed")
else
    for part = parts
        os.output(part)
```

## Crypto Library (crypto)
The Crypto library provides cryptographic functions for hashing, HMAC, key derivation, AES encryption, and secure random generation. It supports MD5, SHA-1, SHA-256, SHA-512, and AES-128-CBC. Import it with `import crypto`.

### crypto.random_hex(nbytes)
Returns a hexadecimal string representation of `nbytes` random bytes generated using a cryptographically secure source. The argument `nbytes` is required and must be a positive integer. Returns `none` on failure or if `nbytes <= 0`.

```apex
import os
import crypto

token = crypto.random_hex(16)

if token != none
    os.output("Hex Token: {token}")
```

### crypto.random_integer(n)
Returns a secure random integer in the range `[0, n)`. Uses a cryptographically secure source. Returns `none` on failure or if `n <= 0`.

```apex
import os
import crypto

val = crypto.random_integer(100)

if val != none
    os.output("Secure random int: {val}")
```

### crypto.random_float()
Returns a cryptographically secure random floating-point number in the range `[0, 1)`. The function uses 53 bits of randomness for uniform distribution. Takes no arguments. Always succeeds.

```apex
import os
import crypto

val = crypto.random_float()

if val != none
    os.output("Secure random float: {val}")
```

### crypto.compare_strings(a, b)
Compares two strings in constant time to prevent timing attacks. Useful for comparing security tokens or hashes. Returns `true` if they match, `false` otherwise. Both arguments must be strings. Always succeeds.

```apex
import os
import crypto

secret = "my_secret_token"
input = "my_secret_token"

if crypto.compare_strings(secret, input) == true
    os.output("Access granted")
else
    os.output("Access denied")
```

### crypto.md5(str)
Computes the MD5 hash of a string. Returns a 32-character lowercase hexadecimal string (128 bits). Returns `none` if the argument is not a string.

```apex
import os
import crypto

file = os.read("main.c")
hash = crypto.md5(file)

if hash == none
    os.output("Could not hash file")
else
    os.output("MD5: {hash}")
```

### crypto.sha1(str)
Computes the SHA-1 hash of a string. Returns a 40-character lowercase hexadecimal string (160 bits). Returns `none` if the argument is not a string.

```apex
import os
import crypto

file = os.read("main.c")
hash = crypto.sha1(file)

if hash == none
    os.output("Could not hash file")
else
    os.output("SHA-1: {hash}")
```

### crypto.sha256(str)
Computes the SHA-256 hash of a string. Returns a 64-character lowercase hexadecimal string (256 bits). Returns `none` if the argument is not a string.

```apex
import os
import crypto

file = os.read("main.c")
hash = crypto.sha256(file)

if hash == none
    os.output("Could not hash file")
else
    os.output("SHA-256: {hash}")
```

### crypto.sha384(str)
Computes the SHA-384 hash of a string. Returns a 96-character lowercase hexadecimal string (384 bits). Returns `none` if the argument is not a string.

```apex
import os
import crypto

file = os.read("main.c")
hash = crypto.sha384(file)

if hash == none
    os.output("Could not hash file")
else
    os.output("SHA-384: {hash}")
```

### crypto.sha512(str)
Computes the SHA-512 hash of a string. Returns a 128-character lowercase hexadecimal string (512 bits). Returns `none` if the argument is not a string.

```apex
import os
import crypto

file = os.read("main.c")
hash = crypto.sha512(file)

if hash == none
    os.output("Could not hash file")
else
    os.output("SHA-512: {hash}")
```

### crypto.hmac_md5(key, msg)
Computes the HMAC MD5 of a message using a secret key. Both arguments must be strings. Returns a 32-character lowercase hexadecimal string. Returns `none` if either argument is not a string.

```apex
import os
import crypto

secret = "my-secret-key"
file = os.read("main.c")
signature = crypto.hmac_md5(secret, file)

if signature == none
    os.output("Could not sign file")
else
    os.output("HMAC MD5: {signature}")
```

### crypto.hmac_sha1(key, msg)
Computes the HMAC SHA1- of a message using a secret key. Both arguments must be strings. Returns a 40-character lowercase hexadecimal string. Returns `none` if either argument is not a string.

```apex
import os
import crypto

secret = "my-secret-key"
file = os.read("main.c")
signature = crypto.hmac_sha1(secret, file)

if signature == none
    os.output("Could not sign file")
else
    os.output("HMAC SHA-1: {signature}")
```

### crypto.hmac_sha256(key, msg)
Computes the HMAC SHA-256 of a message using a secret key. Both arguments must be strings. Returns a 64-character lowercase hexadecimal string. Returns `none` if either argument is not a string.

```apex
import os
import crypto

secret = "my-secret-key"
file = os.read("main.c")
signature = crypto.hmac_sha256(secret, file)

if signature == none
    os.output("Could not sign file")
else
    os.output("HMAC SHA-256: {signature}")
```

### crypto.hmac_sha384(key, msg)
Computes the HMAC SHA-384 of a message using a secret key. Both arguments must be strings. Returns a 96-character lowercase hexadecimal string. Returns `none` if either argument is not a string.

```apex
import os
import crypto

secret = "my-secret-key"
file = os.read("main.c")
signature = crypto.hmac_sha384(secret, file)

if signature == none
    os.output("Could not sign file")
else
    os.output("HMAC SHA-384: {signature}")
```

### crypto.hmac_sha512(key, msg)
Computes the HMAC SHA-512 of a message using a secret key. Both arguments must be strings. Returns a 128-character lowercase hexadecimal string. Returns `none` if either argument is not a string.

```apex
import os
import crypto

secret = "my-secret-key"
file = os.read("main.c")
signature = crypto.hmac_sha512(secret, file)

if signature == none
    os.output("Could not sign file")
else
    os.output("HMAC SHA-512: {signature}")
```

### crypto.pbkdf2_md5(password, salt, iterations, key_len)
Derives a key from a password and salt using PBKDF2-HMAC-MD5. Returns a lowercase hexadecimal string of `key_len` bytes. Returns `none` if any argument is invalid or `iterations < 1`.

```apex
import os
import crypto

key = crypto.pbkdf2_md5("password", "salt", 100000, 16)

if key == none
    os.output("Could not derive key")
else
    os.output("PBKDF2 MD5: {key}")
```

### crypto.pbkdf2_sha1(password, salt, iterations, key_len)
Derives a key from a password and salt using PBKDF2-HMAC-SHA1. Returns a lowercase hexadecimal string of `key_len` bytes. Returns `none` if any argument is invalid or `iterations < 1`.

```apex
import os
import crypto

key = crypto.pbkdf2_sha1("password", "salt", 100000, 20)

if key == none
    os.output("Could not derive key")
else
    os.output("PBKDF2 SHA-1: {key}")
```

### crypto.pbkdf2_sha256(password, salt, iterations, key_len)
Derives a key from a password and salt using PBKDF2-HMAC-SHA256. Returns a lowercase hexadecimal string of `key_len` bytes. Returns `none` if any argument is invalid or `iterations < 1`.

```apex
import os
import crypto

key = crypto.pbkdf2_sha256("password", "salt", 100000, 32)

if key == none
    os.output("Could not derive key")
else
    os.output("PBKDF2 SHA-256: {key}")
```

### crypto.pbkdf2_sha384(password, salt, iterations, key_len)
Derives a key from a password and salt using PBKDF2-HMAC-SHA384. Returns a lowercase hexadecimal string of `key_len` bytes. Returns `none` if any argument is invalid or `iterations < 1`.

```apex
import os
import crypto

key = crypto.pbkdf2_sha384("password", "salt", 100000, 48)

if key == none
    os.output("Could not derive key")
else
    os.output("PBKDF2 SHA-384: {key}")
```

### crypto.pbkdf2_sha512(password, salt, iterations, key_len)
Derives a key from a password and salt using PBKDF2-HMAC-SHA512. Returns a lowercase hexadecimal string of `key_len` bytes. Returns `none` if any argument is invalid or `iterations < 1`.

```apex
import os
import crypto

key = crypto.pbkdf2_sha512("password", "salt", 100000, 64)

if key == none
    os.output("Could not derive key")
else
    os.output("PBKDF2 SHA-512: {key}")
```


### crypto.aes128_encrypt(key, plaintext, iv)
Encrypts a string using AES-128-CBC. The `key` must be a 32-character hexadecimal string (16 bytes). The `plaintext` is the string to encrypt. The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; defaults to all zeros. Returns the encrypted data as a lowercase hexadecimal string. Uses PKCS7 padding. Returns `none` if any argument is invalid.

```apex
import os
import crypto

key = crypto.random_hex(16)
iv = crypto.random_hex(16)
plaintext = "Hello World!"

encrypted = crypto.aes128_encrypt(key, plaintext, iv)

if encrypted == none
    os.output("Could not encrypt data")
else
    os.output("Encrypted: {encrypted}")
```

### crypto.aes128_decrypt(key, ciphertext, iv)
Decrypts a string previously encrypted with AES-128-CBC. The `key` must be a 32-character hexadecimal string (16 bytes). The `ciphertext` must be a hexadecimal string (as returned by `aes128_encrypt`). The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; must match the IV used during encryption. Returns the decrypted plaintext string. Validates PKCS7 padding. Returns `none` if any argument is invalid or if the padding is corrupted (wrong key or IV).

```apex
import os
import crypto

key = crypto.random_hex(16)
iv = crypto.random_hex(16)
plaintext = "Secret message"

encrypted = crypto.aes128_encrypt(key, plaintext, iv)
decrypted = crypto.aes128_decrypt(key, encrypted, iv)

if decrypted == none
    os.output("Could not decrypt data")
else
    os.output("Decrypted: {decrypted}")
```

### crypto.aes192_encrypt(key, plaintext, iv)
Encrypts a string using AES-192-CBC. The `key` must be a 48-character hexadecimal string (24 bytes). The `plaintext` is the string to encrypt. The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; defaults to all zeros. Returns the encrypted data as a lowercase hexadecimal string. Uses PKCS7 padding. Returns `none` if any argument is invalid.

```apex
import os
import crypto

key = crypto.random_hex(24)
iv = crypto.random_hex(16)
plaintext = "Hello World!"

encrypted = crypto.aes192_encrypt(key, plaintext, iv)

if encrypted == none
    os.output("Could not encrypt data")
else
    os.output("Encrypted: {encrypted}")
```

### crypto.aes192_decrypt(key, ciphertext, iv)
Decrypts a string previously encrypted with AES-192-CBC. The `key` must be a 48-character hexadecimal string (24 bytes). The `ciphertext` must be a hexadecimal string (as returned by `aes192_encrypt`). The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; must match the IV used during encryption. Returns the decrypted plaintext string. Validates PKCS7 padding. Returns `none` if any argument is invalid or if the padding is corrupted (wrong key or IV).

```apex
import os
import crypto

key = crypto.random_hex(24)
iv = crypto.random_hex(16)
plaintext = "Secret message"

encrypted = crypto.aes192_encrypt(key, plaintext, iv)
decrypted = crypto.aes192_decrypt(key, encrypted, iv)

if decrypted == none
    os.output("Could not decrypt data")
else
    os.output("Decrypted: {decrypted}")
```

### crypto.aes256_encrypt(key, plaintext, iv)
Encrypts a string using AES-256-CBC. The `key` must be a 64-character hexadecimal string (32 bytes). The `plaintext` is the string to encrypt. The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; defaults to all zeros. Returns the encrypted data as a lowercase hexadecimal string. Uses PKCS7 padding. Returns `none` if any argument is invalid.

```apex
import os
import crypto

key = crypto.random_hex(32)
iv = crypto.random_hex(16)
plaintext = "Hello World!"

encrypted = crypto.aes256_encrypt(key, plaintext, iv)

if encrypted == none
    os.output("Could not encrypt data")
else
    os.output("Encrypted: {encrypted}")
```

### crypto.aes256_decrypt(key, ciphertext, iv)
Decrypts a string previously encrypted with AES-256-CBC. The `key` must be a 64-character hexadecimal string (32 bytes). The `ciphertext` must be a hexadecimal string (as returned by `aes256_encrypt`). The `iv` is optional and must be a 32-character hexadecimal string (16 bytes) if provided; must match the IV used during encryption. Returns the decrypted plaintext string. Validates PKCS7 padding. Returns `none` if any argument is invalid or if the padding is corrupted (wrong key or IV).

```apex
import os
import crypto

key = crypto.random_hex(32)
iv = crypto.random_hex(16)
plaintext = "Secret message"

encrypted = crypto.aes256_encrypt(key, plaintext, iv)
decrypted = crypto.aes256_decrypt(key, encrypted, iv)

if decrypted == none
    os.output("Could not decrypt data")
else
    os.output("Decrypted: {decrypted}")
```