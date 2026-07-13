# Apex Library Reference
Apex comes with several built-in libraries. These are ready-to-use tools that solve common tasks: you don't need to write everything from scratch — just import the library you need and use it.

**Important:** All functions evaluate to `false` on error. Handle it with `if var == false` condition check.

## OS Library (os)
The OS library lets you interact with the operating system, manage processes, and handle standard I/O. Import it with `import os`.

### Basic I/O
#### os.output(value)
Prints a value to the terminal followed by a newline.

```apex
import os

os.output("Hello, Friend")     // Hello, Friend
os.output(42)                  // 42
os.output(true)                // true
```

#### os.input(prompt)
Waits for the user to type something and press Enter. Returns what they typed as a string. You can provide an optional prompt message. The value from `os.input()` is always a string — even if the user types numbers. Use `number()` to convert it if you need to do math. If the user enters something that isn't a number, `number()` returns `false` on failure.

```apex
import os

name = os.input("What is your name? ")

if name == false
    os.output("Could not read input")
else
    os.output("Hello, {name}")
```

### Time and Process Control
#### os.time()
Returns the current time as a number — seconds since January 1, 1970 (with microsecond precision). Useful for measuring how long something takes.

```apex
import os

start = os.time()

if start == false
    os.output("Could not get system time")
else
    end = os.time()
    elapsed = end - start
    os.output("Took {elapsed} seconds")
```

#### os.wait(seconds)
Pauses the program for the given number of seconds. You can use decimals for fractions of a second. Negative values are treated as `0`.

```apex
import os

os.output("Starting...")

result = os.wait(2.5)

if result == false
    os.output("Wait interrupted")
else
    os.output("Done waiting")
```

#### os.exit(code)
Exits the program immediately. The `code` is optional — `0` means success, other numbers mean an error. If no code is given, uses `0`.

```apex
import os

if os.exists("critical_file.txt") == false
    os.output("Critical file missing — exiting")
    os.exit(1)
else
    os.output("Critical file found, continuing...")
```

### Directory Navigation
#### os.get_current_folder()
Returns the current directory as a string — where your program is running from. Returns `false` on failure.

```apex
import os

current_folder = os.get_current_folder()

if current_folder == false
    os.output("Could not get current directory")
else
    os.output("Running from: {current_folder}")
```

#### os.set_current_folder(path)
Changes the current directory. Returns `true` on success, `false` on failure.

```apex
import os

result = os.set_current_folder("/home/user/projects")

if result == false
    os.output("Could not change directory")
else
    os.output("Directory changed successfully")
```

### Process Management
#### os.terminate_process(pid)
Terminates the process with the given PID. Returns `true` on success, `false` on failure.

```apex
import os

result = os.terminate_process(12345)

if result == false
    os.output("Could not terminate process")
else
    os.output("Process terminated successfully")
```

#### os.execute(command)
Runs a system command as if you typed it in the terminal. Returns the command's exit code. Available commands depend on your operating system. Returns `-1` on failure to execute.

```apex
import os

exit_code = os.execute("echo Hello from terminal")

if exit_code == -1
    os.output("Could not execute command")
else
    os.output("Command exited with code: {exit_code}")
```

## Files Library (files)
The Files functionality is integrated into the `os` library. Import it with `import os`.

### File Read/Write
#### os.read(filename)
Reads the entire contents of a file and returns it as a string. Returns `false` if the file cannot be read.

```apex
import os

content = os.read("story.txt")

if content == false
    os.output("Could not read the file")
else
    os.output(content)
```

#### os.write(filename, content)
Writes content to a file. Creates the file if it doesn't exist, overwrites it if it does. Returns `true` on success, `false` on failure.

```apex
import os

success = os.write("notes.txt", "Today I learned Apex!")

if success == false
    os.output("Could not write to the file")
else
    os.output("File saved successfully")
```

#### os.append(filename, content)
Appends content to the end of a file. Creates the file if it doesn't exist. Returns `true` on success, `false` on failure.

```apex
import os

result = os.append("log.txt", "Second line\n")

if result == false
    os.output("Could not append to the file")
else
    os.output("Second line appended")
```

### File Properties
#### os.exists(path)
Returns `true` if the file or folder at `path` exists, `false` otherwise.

```apex
import os

exists = os.exists("config.apex")

if exists == false
    os.output("No config file")
else
    os.output("Config file found")
```

#### os.isfile(path)
Returns `true` if the path points to a file specifically (not a folder). Returns `false` otherwise.

```apex
import os

result = os.isfile("data.txt")

if result == false
    os.output("Not a file or doesn't exist")
else
    os.output("It's a file")
```

#### os.isfolder(path)
Returns `true` if the path points to a folder specifically. Returns `false` otherwise.

```apex
import os

result = os.isfolder("my_folder")

if result == false
    os.output("Not a folder or doesn't exist")
else
    os.output("It's a folder")
```

#### os.size(path)
Returns the size of a file or directory in bytes. For directories, it calculates the total size recursively. Returns a number on success, `false` if the path doesn't exist.

```apex
import os

size = os.size("movie.mp4")

if size == false
    os.output("Could not get size")
else
    os.output("Size: {size} bytes")
```

#### os.filetype(path)
Detects the file type by its contents (magic bytes). Returns a string describing the type, or `false` if the file is not found. Recognizes: `PDF document`, `PNG image`, `JPEG image`, `GIF image`, `ZIP archive`, `ELF executable`, `Windows executable`, `Plain text`, `Unknown binary`.

```apex
import os

filetype = os.filetype("document.pdf")

if filetype == false
    os.output("Could not detect file type")
else
    os.output("File type: {filetype}")
```

#### os.stat(path)
Returns a table with information about a file or folder. The table contains these keys:
- `size` — file size in bytes
- `mtime` — last modification time (timestamp)
- `ctime` — creation time (timestamp)
- `isdir` — `true` if it's a folder, `false` if it's a file

Returns a table on success, `false` on failure.

```apex
import os

info = os.stat("data.txt")

if info == false
    os.output("File does not exist")
else
    os.output("Size: {info["size"]} bytes")
    os.output("Modified: {info["mtime"]}")
    os.output("Created: {info["ctime"]}")
    os.output("Is directory: {info["isdir"]}")
```

### Rename and Move
#### os.rename(old_name, new_name)
Renames a file or directory. Returns `true` on success, `false` on failure.

```apex
import os

result = os.rename("old.txt", "new.txt")

if result == false
    os.output("Could not rename")
else
    os.output("Renamed successfully")
```

#### os.move(source, destination)
Moves a file or directory to a new location. Returns `true` on success, `false` on failure.

```apex
import os

result = os.move("doc.txt", "backup/doc.txt")

if result == false
    os.output("Could not move")
else
    os.output("Moved successfully")
```

### Copy
#### os.copy(source, destination)
Copies a file or recursively copies a directory with all its contents. Returns `true` on success, `false` on failure.

```apex
import os

result = os.copy("original.txt", "copy.txt")

if result == false
    os.output("Could not copy")
else
    os.output("Copied successfully")
```

### Create and Delete
#### os.create_file(filename)
Creates an empty file. Returns `true` on success, `false` on failure.

```apex
import os

result = os.create_file("new_file.txt")

if result == false
    os.output("Could not create the file")
else
    os.output("File created successfully")
```

#### os.create_folder(path)
Creates a new folder. Returns `true` on success, `false` if the folder already exists or can't be created.

```apex
import os

result = os.create_folder("my_project")

if result == false
    os.output("Folder already exists or can't be created")
else
    os.output("Folder created successfully")
```

#### os.delete(path)
Deletes a file or an empty folder. Returns `true` if deleted, `false` if the path doesn't exist, is a non-empty folder, or can't be deleted.

```apex
import os

result = os.delete("temp.txt")

if result == false
    os.output("Could not delete")
else
    os.output("Deleted successfully")
```

### Directory Navigation
#### os.items(path)
Returns a table of names — all files and folders inside the given folder. If no path is given, lists the current folder. Returns `false` on failure.

```apex
import os

items = os.items(".")

if items == false
    os.output("Could not list directory contents")
else
    os.output("Contents of current directory:")
    for item in items
        os.output("  {item}")
```

#### os.parentfolder(path)
Returns the parent directory of the given path. For root paths like `/` or `C:\`, returns the root itself. If no directory separator is found in the path, returns `"."`. Returns `false` if no path is provided or on failure.

```apex
import os

parent = os.parentfolder("/home/user/projects")

if parent == false
    os.output("Could not get parent folder")
else
    os.output("Parent folder: {parent}")
```

### Permissions
#### os.access(path, mode)
Changes file permissions. The `mode` is a number (e.g., `755` for rwxr-xr-x on Unix). Returns `true` on success, `false` on failure.

```apex
import os

result = os.access("script.sh", 755)

if result == false
    os.output("Could not change permissions")
else
    os.output("Permissions changed successfully")
```

## System Library (sys)
The System library provides static or rarely changing system information. Import it with `import sys`.

### System Info
#### sys.platform()
Returns a string identifying your operating system, such as `"Windows"`, `"macOS"`, `"iOS"`, `"tvOS"`, `"watchOS"`, `"Android"`, `"Linux"`, `"FreeBSD"`, `"OpenBSD"`, `"NetBSD"`, `"QNX"`, or `"Unix"`. Returns `none` if the platform cannot be detected.

```apex
import os
import sys

system = sys.platform()

if system == none
    os.output("Could not detect platform")
else
    os.output("You're running on {system}")
```

#### sys.architecture()
Returns a string identifying the system's processor architecture. Possible return values:

- `"x86-64"` — 64-bit x86 (AMD64/Intel 64)
- `"arm64"` — 64-bit ARM (includes systems where `uname -m` reports `aarch64`, mapped to `"arm64"`)
- `"x86"` — 32-bit x86
- `"arm"` — 32-bit ARM

Returns `none` if the architecture cannot be detected.

```apex
import os
import sys

arch = sys.architecture()

if arch == none
    os.output("Could not get architecture")
else
    os.output("System Architecture: {arch}")
```

#### sys.hostname()
Returns the system's hostname as a string. Returns `none` on failure.

```apex
import os
import sys

name = sys.hostname()

if name == none
    os.output("Could not get hostname")
else
    os.output("Hostname: {name}")
```

#### sys.user()
Returns the current user's login name as a string. Returns `none` if it can't be determined.

```apex
import os
import sys

username = sys.user()

if username == none
    os.output("Could not get username")
else
    os.output("Logged in as: {username}")
```

#### sys.homedir()
Returns the current user's home directory path as a string. Returns `none` if it can't be determined.

```apex
import os
import sys

home = sys.homedir()

if home == none
    os.output("Could not get home directory")
else
    os.output("Home folder: {home}")
```

#### sys.isterminal(fd)
Checks if the file descriptor goes to a terminal. By default checks stdout (fd 1). Pass a number to check a different file descriptor: `0` for stdin, `2` for stderr. Returns `true` if the output goes to a terminal, `false` if it's redirected to a file or pipe. Always succeeds.

```apex
import os
import sys

if sys.isterminal() == true
    os.output("Output is a terminal")
else
    os.output("Output is not a terminal")
```

#### sys.apex_version()
Returns the current Apex interpreter version as a string. Always succeeds.

```apex
import os
import sys

version = sys.apex_version()
os.output("Apex Version: {version}")
```

#### sys.executable()
Returns the full path to the currently running Apex executable. Returns `none` on failure.

```apex
import os
import sys

path = sys.executable()

if path == none
    os.output("Could not get executable path")
else
    os.output("Running from: {path}")
```

#### sys.disksize(path)
Returns a table with disk usage information for the volume containing the given path. If no path is provided, uses the current directory. The table contains:
- `total` — Total size in MB
- `used` — Used space in MB
- `free` — Free space in MB

Returns a table on success, `none` on failure.

```apex
import os
import sys

info = sys.disksize(".")

if info == none
    os.output("Could not get disk info")
else
    os.output("Total: {info["total"]} MB")
    os.output("Free: {info["free"]} MB")
```

#### sys.tempdir()
Returns the path to the system's temporary directory. On Unix-like systems, checks the `TMPDIR`, `TMP`, and `TEMP` environment variables, falling back to `/tmp`. Returns `none` on failure.

```apex
import os
import sys

temp_dir = sys.tempdir()

if temp_dir == none
    os.output("Could not get temporary directory")
else
    os.output("Temporary directory: {temp_dir}")
```

#### sys.environment()
Returns a table containing all environment variables as key-value pairs. Always returns a table (empty if no variables).

```apex
import os
import sys

env_vars = sys.environment()
os.output("HOME = {env_vars["HOME"]}")
os.output("PATH = {env_vars["PATH"]}")
```

#### sys.process_id()
Returns the current process ID as a number. Always succeeds.

```apex
import os
import sys

pid = sys.process_id()
os.output("Process ID: {pid}")
```

## Math Library (math)
The Math library provides mathematical functions beyond basic arithmetic. Import it with `import math`.

### Constants
#### math.pi()
Returns the mathematical constant π (pi), approximately 3.14159. Pi represents the ratio of a circle's circumference to its diameter.

```apex
import math
math.pi()    // 3.141592653589793
```

#### math.e
Returns Euler's number (e), approximately 2.71828. This constant is the base of natural logarithms and appears in growth rates, compound interest, and many natural processes.

```apex
import math
math.e()    // 2.718281828459045
```

#### math.inf
Returns positive infinity. This represents a value larger than any finite number. Useful for comparisons and algorithm boundaries.

```apex
import math
math.inf()    // inf
```

### Rounding and Truncation
#### math.abs(x)
Returns the absolute value of a number — how far it is from zero, ignoring the sign. Returns `none` if the argument is not a number.

```apex
import math

math.abs(5)        // 5
math.abs(-5)       // 5
math.abs(-3.14)    // 3.14
```

#### math.floor(x)
Rounds a number down to the nearest whole number. Returns `none` if the argument is not a number.

```apex
import math

math.floor(3.7)     // 3
math.floor(3.1)     // 3
math.floor(-2.3)    // -3 (goes down, so more negative)
```

#### math.ceil(x)
Rounds a number up to the nearest whole number. Returns `none` if the argument is not a number.

```apex
import math

math.ceil(3.1)      // 4
math.ceil(3.7)      // 4
math.ceil(-2.3)     // -2 (goes up toward zero)
```

#### math.round(x, digits)
Rounds a number to the nearest whole number, or to a specific number of decimal places. The `digits` parameter is optional — without it, rounds to a whole number. At exactly .5, rounds up. Returns `none` on error.

```apex
import math

math.round(3.4)         // 3
math.round(3.6)         // 4
math.round(3.5)         // 4 (rounds up at .5)
math.round(3.14159, 2)  // 3.14
math.round(3.14159, 3)  // 3.142
```

#### math.trunc(x)
Truncates a number by removing the decimal part. Returns `none` if the argument is not a number.

```apex
import math

math.trunc(3.7)     // 3
math.trunc(-3.7)    // -3
math.trunc(0.9)     // 0
```

### Powers and Roots
#### math.sqrt(x)
Returns the square root of a number. Returns `NaN` for negative numbers. Returns `none` if the argument is not a number. You can check the result with `math.isnan()`.

```apex
import math

math.sqrt(25)      // 5
math.sqrt(2)       // 1.4142135623730951
math.sqrt(-1)      // NaN
```

#### math.pow(base, exponent)
Raises a number to a power. Returns `base` raised to the `exponent`. Returns `none` on error.

```apex
import math

math.pow(2, 3)     // 8
math.pow(4, 0.5)   // 2 (same as square root)
math.pow(10, -1)   // 0.1
```

#### math.exp(x)
Returns `e` raised to the power of `x`. Returns `none` if the argument is not a number.

```apex
import math

math.exp(1)        // 2.718281828459045
math.exp(0)        // 1
math.exp(2)        // 7.38905609893065
```

#### math.hypot(x, y)
Returns the hypotenuse of a right triangle given the lengths of the two legs. Returns `none` on error.

```apex
import math

math.hypot(3, 4)   // 5
math.hypot(5, 12)  // 13
math.hypot(1, 1)   // 1.4142135623730951
```

### Logarithms
#### math.log(x, base)
Returns the logarithm of `x`. Without a base, uses the natural logarithm (base `e`). With a base, calculates the logarithm with that base. Returns `NaN` for zero or negative inputs. Returns `none` on error.

```apex
import math

math.log(2.718281828459045)  // ~1 (natural log of e)
math.log(100, 10)            // 2 (10² = 100)
math.log(8, 2)               // 3 (2³ = 8)
math.log(0)                  // NaN
math.log(-5)                 // NaN
```

### Trigonometry
All trigonometric functions work with **radians**, not degrees. Use `math.radians()` and `math.degrees()` to convert between them. All functions return `none` on error.

#### math.sin(x)
Returns the sine of `x` radians.

```apex
import math

math.sin(0)                // 0
math.sin(math.pi() / 2)    // 1
math.sin(math.pi())        // ~0
```

#### math.cos(x)
Returns the cosine of `x` radians.

```apex
import math

math.cos(0)                // 1
math.cos(math.pi() / 2)    // ~0
math.cos(math.pi())        // -1
```

#### math.tan(x)
Returns the tangent of `x` radians.

```apex
import math

math.tan(0)                // 0
math.tan(math.pi() / 4)    // ~1
```

#### math.asin(x)
Returns the arcsine of `x` in radians. Input must be between -1 and 1. Returns `NaN` for values outside this range. Returns `none` on error.

```apex
import math

math.asin(0)               // 0
math.asin(1)               // 1.5707963267948966 (π/2)
math.asin(2)               // NaN
```

#### math.acos(x)
Returns the arccosine of `x` in radians. Input must be between -1 and 1. Returns `NaN` for values outside this range. Returns `none` on error.

```apex
import math

math.acos(1)               // 0
math.acos(0)               // 1.5707963267948966 (π/2)
math.acos(2)               // NaN
```

#### math.atan(x)
Returns the arctangent of `x` in radians. Returns `none` on error.

```apex
import math

math.atan(0)               // 0
math.atan(1)               // 0.7853981633974483 (π/4)
```

#### math.atan2(y, x)
Returns the arctangent of `y/x` in radians, using the signs of both arguments to determine the correct quadrant. Returns `none` on error.

```apex
import math

math.atan2(1, 1)           // 0.7853981633974483 (π/4)
math.atan2(1, 0)           // 1.5707963267948966 (π/2)
math.atan2(-1, 0)          // -1.5707963267948966 (-π/2)
```

### Angle Conversion
#### math.radians(degrees)
Converts degrees to radians. Returns `none` on error.

```apex
import math

math.radians(180)          // 3.141592653589793 (π)
math.radians(90)           // 1.5707963267948966 (π/2)
math.radians(360)          // 6.283185307179586 (2π)
```

#### math.degrees(radians)
Converts radians to degrees. Returns `none` on error.

```apex
import math

math.degrees(math.pi())      // 180
math.degrees(math.pi() / 2)  // 90
math.degrees(1)              // 57.29577951308232
```

### Number Theory
#### math.gcd(a, b)
Returns the greatest common divisor of two numbers. Both arguments are converted to positive integers before calculation. Returns `none` on error.

```apex
import math

math.gcd(12, 8)            // 4
math.gcd(17, 5)            // 1
math.gcd(48, 18)           // 6
```

#### math.factorial(n)
Returns the factorial of a non-negative integer `n` (the product of all positive integers from 1 to `n`). The maximum allowed input is 170 — larger values will return `inf`. Returns `none` on error.

```apex
import math

math.factorial(0)          // 1
math.factorial(1)          // 1
math.factorial(5)          // 120
math.factorial(10)         // 3628800
math.factorial(-3)         // none
math.factorial(2.5)        // none
math.factorial(171)        // inf (too large)
```

### Number Testing
#### math.isnan(x)
Returns `true` if the value is NaN (Not a Number), `false` otherwise. NaN typically results from operations like `sqrt(-1)` or `0/0`. Returns `none` if the argument is not a number.

```apex
import math

math.isnan(math.sqrt(-1))  // true
math.isnan(0)              // false
math.isnan(42)             // false
```

#### math.isinf(x)
Returns `true` if the value is positive or negative infinity, `false` otherwise. Infinity often results from division by zero or overflowing calculations. Returns `none` if the argument is not a number.

```apex
import math

math.isinf(math.inf())     // true
math.isinf(-math.inf())    // true
math.isinf(1000)           // false
math.isinf(0)              // false
```

## String Library (string)
The String library helps you work with text — measure length, change case, find words, split and combine strings, and more. Import it with `import string`.

### string.len(s)
Returns the number of characters in a string. Spaces and punctuation count as characters too. Returns `none` if the argument is not a string.

```apex
import string

result = string.len("hello")
if result != none
    os.output(result)        // 5

string.len("")               // 0
string.len("hi there")       // 8 (space counts)
string.len("Apex!")          // 5
```

### string.lower(s)
Converts every character in the string to lowercase. Returns `none` if the argument is not a string.

```apex
import string

string.lower("HELLO")        // "hello"
string.lower("Hello")        // "hello"
string.lower("Apex 123")     // "apex 123"
```

### string.upper(s)
Converts every character in the string to uppercase. Returns `none` if the argument is not a string.

```apex
import string

string.upper("hello")        // "HELLO"
string.upper("Hello")        // "HELLO"
string.upper("apex 123")     // "APEX 123"
```

### string.sub(s, start, end)
Extracts a portion of a string — from `start` to `end`, but not including `end`. Positions start counting from `0`. Returns `none` on error.

```apex
import string

text = "Hello, World"
result = string.sub(text, 0, 5)
if result != none
    os.output(result)        // "Hello"

string.sub(text, 7, 12)      // "World"
string.sub(text, 0, 1)       // "H" (first character only)
```

If `start` is negative, it's treated as `0`. If `end` is larger than the string length, it stops at the end.

```apex
import string

string.sub("Apex", 1, 10)    // "pex" (end is bigger than string, stops at end)
```

### string.split(s, separator)
Splits a string into a table of substrings. The separator is the character (or characters) where the split happens. If no separator is given, splits on whitespace. Returns `none` if the argument is not a string.

```apex
import string

result = string.split("apple,banana,orange", ",")
if result != none
    os.output(result["1"])   // "apple"

string.split("hello world apex", " ")   // ["hello", "world", "apex"]
string.split("one two three")           // ["one", "two", "three"]
string.split("word")                    // ["word"]
```

Practical example — processing user input:

```apex
import os
import string

user_input = os.input("Enter three numbers separated by commas: ")
parts = string.split(user_input, ",")

if parts == none
    os.output("Invalid input")
else
    os.output("First number: {parts["1"]}")
    os.output("Second number: {parts["2"]}")
    os.output("Third number: {parts["3"]}")
```

### string.join(parts, separator)
Does the opposite of `split` — takes a table of strings and joins them into one string with a separator between each. The separator is optional. Returns `none` if the first argument is not a table.

```apex
import string

words = ["Hello", "World"]
result = string.join(words, " ")
if result != none
    os.output(result)        // "Hello World"

string.join(words, "-")      // "Hello-World"
string.join(words)           // "HelloWorld" (no separator)

tags = ["apex", "programming", "language"]
string.join(tags, ", ")      // "apex, programming, language"
```

### string.trim(s)
Removes whitespace (spaces, tabs, newlines) from the beginning and end of a string. The middle spaces are left alone. Returns `none` if the argument is not a string.

```apex
import string

string.trim("  hello  ")          // "hello"
string.trim("   apex   lang   ")  // "apex   lang" (inner spaces kept)
string.trim("\n  text \n")        // "text"
```

Very useful when cleaning user input:

```apex
import os
import string

name = os.input("Enter your name: ")
name = string.trim(name)

if name == none
    os.output("Invalid input")
else
    os.output("Hello, {name}")
```

### string.find(s, search)
Searches for `search` inside `s` and returns the position of the first match. Returns `-1` if not found. Position starts from `0`. Returns `none` on error.

```apex
import string

text = "Hello, World"
result = string.find(text, "World")
if result != none
    os.output(result)        // 7

string.find(text, "o")       // 4 (first 'o' is at position 4)
string.find(text, "Apex")    // -1 (not found)
```

You can use the result to check if something exists in a string:

```apex
import os
import string

email = "alice@example.com"
pos = string.find(email, "@")

if pos == none
    os.output("Error checking email")
else if pos != -1
    os.output("Valid email format")
else
    os.output("Missing @ symbol")
```

### string.replace(s, old, new)
Replaces the **first** occurrence of `old` with `new` in the string. If `old` isn't found, returns the original string unchanged. Returns `none` on error.

```apex
import string

result = string.replace("Hello World", "World", "Apex")
if result != none
    os.output(result)        // "Hello Apex"

string.replace("banana", "a", "o")          // "bonana" (only first 'a')
string.replace("hello", "x", "y")           // "hello" (nothing changed)
string.replace("remove-this", "-this", "")  // "remove"
```

### string.isletter(s)
Checks if the first character of the string is a letter (supports all modern writing systems: Latin, Cyrillic, Arabic, Chinese, Japanese, Korean, Hebrew, Greek, Devanagari, and many more). Returns `none` on error.

```apex
import string

string.isletter("Hello")    // true
string.isletter("Привет")   // true (Cyrillic)
string.isletter("مرحبا")    // true (Arabic)
string.isletter("你好")      // true (Chinese)
string.isletter("123")      // false (number)
string.isletter("!")        // false (punctuation)
string.isletter("")         // false (empty string)
```

### string.isnumber(s)
Checks if the first character of the string is a digit (0-9). Returns `none` on error.

```apex
import string

string.isnumber("123")      // true
string.isnumber("5hello")   // true (first char is '5')
string.isnumber("abc")      // false
string.isnumber("")         // false
```

## Table Library (table)
The Table library provides functions for working with tables. Import it with `import table`.

### table.size(t)
Returns the number of items in the table. Always returns a number (0 if empty).

```apex
import table

user = ["name" = "Alice", "age" = 30]
os.output(table.size(user))          // 2

colors = ["red", "green", "blue"]
os.output(table.size(colors))        // 3

empty = []
os.output(table.size(empty))         // 0
```

### table.has(t, key)
Returns `true` if the table has the specified key or index. Returns `none` if the key is not a string.

```apex
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
Removes an item from a table by key. Returns `true` if the key existed and was removed, `false` otherwise. Returns `none` if the key is not a string.

```apex
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
result = table.remove(user, "age")

if result == none
    os.output("Invalid key type")
else if result == true
    os.output("Key removed")
else
    os.output("Key did not exist")
// user is now ["name" = "Alice", "active" = true]

colors = ["red", "green", "blue"]
result = table.remove(colors, 2)

if result == none
    os.output("Invalid key type")
else
    // Note: key "2" doesn't exist in this table structure
    os.output("Removed: {result}")  // false
```

### table.keys(t)
Returns a table of all keys in the table as strings. For ordered items without keys, their positions are converted to strings. Keys are sorted numerically if possible, otherwise lexicographically. Always returns a table (empty if source is empty).

```apex
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
keys = table.keys(user)   // ["name", "age", "active"]

colors = ["red", "green", "blue"]
keys = table.keys(colors) // ["1", "2", "3"]

empty = []
keys = table.keys(empty)  // []
```

### table.values(t)
Returns a table of all values in the table in order. Keys are sorted numerically if possible, otherwise lexicographically. Always returns a table (empty if source is empty).

```apex
import table

user = ["name" = "Alice", "age" = 30, "active" = true]
values = table.values(user)   // ["Alice", 30, true]

colors = ["red", "green", "blue"]
values = table.values(colors) // ["red", "green", "blue"]

empty = []
values = table.values(empty)  // []
```

### table.clear(t)
Removes all items from the table. Always returns `none`.

```apex
import table

user = ["name" = "Alice", "age" = 30]
table.clear(user)         // user is now []
```

### table.copy(t)
Returns a shallow copy of the table. Changes to the copy don't affect the original. Always returns a table.

**Important**: This function may return `false` as a valid value if the source table contains `false`. Always check for `none` to detect errors (though this function never returns `none`).

```apex
import table

original = ["name" = "Alice", "age" = 30]
duplicate = table.copy(original)

if duplicate == none
    os.output("Copy failed")  // never happens
else
    duplicate["name"] = "Bob"
    // original["name"] is still "Alice"
```

### table.merge(t1, t2)
Merges two tables into a new table. If keys conflict, values from the second table overwrite the first. Returns `none` if the second argument is not a table.

**Important**: This function may return `false` as a valid value if the merged table contains `false`. Always check for `none` to detect errors.

```apex
import table

t1 = ["name" = "Alice", "age" = 30]
t2 = ["city" = "Dubai", "age" = 31]
merged = table.merge(t1, t2)

if merged == none
    os.output("Merge failed: second argument is not a table")
else
    // merged is ["name" = "Alice", "age" = 31, "city" = "Dubai"]
```

## FFI Library (ffi)
The FFI library lets you call functions from shared libraries (.so on Linux, .dll on Windows). You can load libraries, call C functions, and manage memory. Import it with `import ffi`.

### Loading Libraries
#### ffi.open(path)
Loads a shared library from the given path and returns a table representing the library. The table contains `_handle` (internal numeric handle) and `path` (the library path). Returns `none` if the library cannot be loaded.

If the path does not contain a slash or backslash, "./" is prepended to search in the current directory.

```apex
import os
import ffi

lib = ffi.open("libc.so.6")

if lib == none
    os.output("Could not load library")
else
    os.output("Library loaded: {lib["path"]}")
```

### Calling Functions
#### ffi.call(lib_table, func_name, ...)
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

### Error Handling
#### ffi.errno()
Returns the current value of `errno` as a number. Always succeeds.

```apex
import os
import ffi

lib = ffi.open("nonexistent.so")

if lib == none
    err = ffi.errno()
    os.output("Error code: {err}")
```

#### ffi.strerror(code)
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

### Memory Management
#### ffi.malloc(size)
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

#### ffi.free(ptr)
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

### Seeding
#### random.seed(value)
Initializes the random number generator with a specific seed value. Using the same seed will produce the same sequence of random numbers, which is useful for reproducibility. If called without arguments, it seeds using the current system time. Always returns `none`.

```apex
import os
import random

random.seed(12345)
r1 = random.random()

random.seed(12345)
r2 = random.random()

if r1 == r2
    os.output("Seeds match!")
```

### Basic Generation
#### random.random()
Returns a random floating-point number in the range `[0.0, 1.0)`. Always succeeds.

```apex
import os
import random

val = random.random()
os.output("Random float: {val}")
```

#### random.randint(a, b)
Returns a random integer `N` such that `a <= N <= b`. If `a > b`, the bounds are swapped automatically. Returns `none` if arguments are not numbers.

```apex
import os
import random

dice = random.randint(1, 6)
if dice != none
    os.output("You rolled a {dice}")
```

### Sequence Operations
#### random.choice(seq)
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

#### random.shuffle(seq)
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

#### random.sample(seq, k)
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

### Distributions
#### random.gauss(mu, sigma)
Returns a random floating-point number from a Gaussian (normal) distribution with mean `mu` and standard deviation `sigma`. Returns `none` if arguments are not numbers.

```apex
import os
import random

height = random.gauss(175, 10)
if height != none
    os.output("Simulated height: {height} cm")
```

#### random.triangular(low, high, mode)
Returns a random floating-point number from a triangular distribution. `low` and `high` default to 0 and 1, while `mode` defaults to the midpoint between them. Returns `none` if arguments are not numbers.

```apex
import os
import random

val = random.triangular(0, 10, 5)
if val != none
    os.output("Triangular sample: {val}")
```

#### random.expovariate(lambd)
Returns a random floating-point number from an exponential distribution with rate parameter `lambd`. Returns `none` if `lambd` is zero or not a number.

```apex
import os
import random

wait_time = random.expovariate(0.5)
if wait_time != none
    os.output("Expected wait: {wait_time} minutes")
```

#### random.betavariate(alpha, beta)
Returns a random floating-point number from a Beta distribution with parameters `alpha` and `beta`. Both parameters must be greater than zero. Returns `none` otherwise.

```apex
import os
import random

probability = random.betavariate(2, 5)
if probability != none
    os.output("Beta sample: {probability}")
```

### Secure Randomness
#### random.secure_token_hex(nbytes)
Returns a hexadecimal string representation of `nbytes` random bytes generated using a cryptographically secure source. Defaults to 16 bytes if no argument is provided. Returns `none` on failure.

```apex
import os
import random

token = random.secure_token_hex(16)

if token == none
    os.output("Could not generate secure token")
else
    os.output("Hex Token: {token}")
```

#### random.secure_randint(n)
Returns a secure random integer in the range `[0, n)`. Uses a cryptographically secure source. Returns `none` on failure or if `n <= 0`.

```apex
import os
import random

val = random.secure_randint(100)

if val == none
    os.output("Could not generate secure random int")
else
    os.output("Secure random int: {val}")
```

#### random.compare_digest(a, b)
Compares two strings in constant time to prevent timing attacks. Useful for comparing security tokens or hashes. Returns `true` if they match, `false` otherwise. Both arguments must be strings. Always succeeds.

```apex
import os
import random

secret = "my_secret_token"
input = "my_secret_token"

if random.compare_digest(secret, input) == true
    os.output("Access granted")
else
    os.output("Access denied")
```

## Codecs Library (codecs)
The Codecs library provides encoding and decoding functions for various formats. Import it with `import codecs`.

### Base64 Encoding
#### codecs.base_write(data)
Encodes a string to standard Base64. Returns the encoded string, or `none` on failure.

```apex
import os
import codecs

encoded = codecs.base_write("Hello World")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)  // SGVsbG8gV29ybGQ=
```

#### codecs.base_read(data)
Decodes a standard Base64 string. Returns the decoded string, or `none` on failure.

```apex
import os
import codecs

decoded = codecs.base_read("SGVsbG8gV29ybGQ=")

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)  // Hello World
```

#### codecs.baseurl_write(data)
Encodes a string to URL-safe Base64 (using `-` and `_` instead of `+` and `/`). Returns the encoded string, or `none` on failure.

```apex
import os
import codecs

encoded = codecs.baseurl_write("Hello World?")

if encoded == none
    os.output("Encoding failed")
else
    os.output(encoded)
```

#### codecs.baseurl_read(data)
Decodes a URL-safe Base64 string. Returns the decoded string, or `none` on failure.

```apex
import os
import codecs

decoded = codecs.baseurl_read(encoded)

if decoded == none
    os.output("Decoding failed")
else
    os.output(decoded)
```

### JSON Encoding
#### codecs.json_write(value)
Converts an Apex value (number, bool, string, table) to a JSON string. Returns the JSON string, or `none` on failure. Tables are encoded as objects `{}` if they have named keys, or arrays `[]` if they only have sequential numeric keys.

```apex
import os
import codecs

data = ["name" = "Alice", "age" = 30]
json_str = codecs.json_write(data)

if json_str == none
    os.output("JSON encoding failed")
else
    os.output(json_str)  // {"name": "Alice", "age": 30}
```

#### codecs.json_read(json_string)
Parses a JSON string into an Apex value. Returns the parsed value (table, number, bool, string), or `none` on failure.

```apex
import os
import codecs

json_str = "{\"name\": \"Alice\", \"age\": 30}"
data = codecs.json_read(json_str)

if data == none
    os.output("JSON parsing failed")
else
    os.output("Name: {data["name"]}")
```

### CSV Encoding
#### codecs.csv_write(table, has_header, delimiter)
Converts a table of tables to a CSV string. `has_header` (bool) determines if the first row is treated as headers. `delimiter` (string) specifies the separator (default `,`). Returns the CSV string, or `none` on failure.

```apex
import os
import codecs

data = [
    1 = ["name" = "Alice", "age" = 30],
    2 = ["name" = "Bob", "age" = 25]
]

csv_str = codecs.csv_write(data, true, ",")

if csv_str == none
    os.output("CSV encoding failed")
else
    os.output(csv_str)
```

#### codecs.csv_read(csv_string, has_header, delimiter)
Parses a CSV string into a table of tables. `has_header` (bool) determines if the first row contains column names. `delimiter` (string) specifies the separator (default `,`). Returns a table of rows, or `none` on failure.

```apex
import os
import codecs

csv_str = "name,age\nAlice,30\nBob,25"
data = codecs.csv_read(csv_str, true, ",")

if data == none
    os.output("CSV parsing failed")
else
    os.output("First name: {data["1"]["name"]}")
```

### XML Encoding
#### codecs.xml_write(table)
Converts a table representing an XML structure to an XML string. The table should have a `__tag` key for the element name, `@key` keys for attributes, and `#text` for text content. Nested elements are stored with numeric keys. Returns the XML string, or `none` on failure.

```apex
import os
import codecs

xml_data = [
    "__tag" = "root",
    "@id" = "1",
    1 = [
        "__tag" = "child",
        "#text" = "Hello"
    ]
]

xml_str = codecs.xml_write(xml_data)

if xml_str == none
    os.output("XML encoding failed")
else
    os.output(xml_str)
```

#### codecs.xml_read(xml_string)
Parses an XML string into a table structure. Returns the root element as a table, or `none` on failure.

```apex
import os
import codecs

xml_str = "<root id='1'><child>Hello</child></root>"
data = codecs.xml_read(xml_str)

if data == none
    os.output("XML parsing failed")
else
    os.output("Tag: {data["__tag"]}")
```