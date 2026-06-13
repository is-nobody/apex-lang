# Apex Library Reference
Apex comes with several built-in libraries. These are ready-to-use tools that solve common tasks: you don't need to write everything from scratch — just import the library you need and use it.

**Important:** All functions evaluate to `false` on error. Handle it with `if var == false` condition check.

## OS Library (os)
The OS library lets you interact with the operating system. You can print text, read files, create folders, and more. Import it with `import os`.

### Basic I/O
#### os.output(value)
Prints a value to the terminal.

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

result = os.write("log.txt", "First line\n")

if result == false
    os.output("Could not write to the file")
else
    os.output("First line written")

result = os.append("log.txt", "Second line\n")

if result == false
    os.output("Could not append to the file")
else
    os.output("Second line appended")
// log.txt now contains both lines
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

#### os.isdir(path)
Returns `true` if the path points to a folder specifically. Returns `false` otherwise.

```apex
import os

result = os.isdir("my_folder")

if result == false
    os.output("Not a folder or doesn't exist")
else
    os.output("It's a folder")
```

#### os.filesize(path)
Returns the size of a file in bytes. Returns a number on success, `false` if the file doesn't exist or isn't a regular file.

```apex
import os

size = os.filesize("movie.mp4")

if size == false
    os.output("Could not get file size")
else
    os.output("File size: {size} bytes")
```

#### os.dirsize(path)
Calculates the total size of a directory in bytes (recursively, including all nested files and folders). Returns a number on success, or `false` if the directory doesn't exist.

```apex
import os

size = os.dirsize("downloads")

if size == false
    os.output("Could not calculate directory size")
else
    os.output("Folder size: {size} bytes")
```

#### os.filetype(path)
Detects the file type by its contents (magic bytes). Returns a string describing the type, or `false` if the file is not found. Recognizes: `PDF`, `PNG`, `JPEG`, `GIF`, `ZIP`, `ELF`, `Windows executable`, `Plain text`, `Unknown binary`.

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
    os.output("Size: {info.size} bytes")
    os.output("Modified: {info.mtime}")
    os.output("Created: {info.ctime}")
    os.output("Is directory: {info.isdir}")
```

### Rename and Move
#### os.rnfile(old_name, new_name)
Renames a file. Returns `true` on success, `false` if the source doesn't exist or isn't a file.

```apex
import os

result = os.rnfile("old.txt", "new.txt")

if result == false
    os.output("Could not rename the file")
else
    os.output("File renamed successfully")
```

#### os.rndir(old_name, new_name)
Renames a directory. Returns `true` on success, `false` if the source doesn't exist or isn't a directory.

```apex
import os

result = os.rndir("old_folder", "new_folder")

if result == false
    os.output("Could not rename the directory")
else
    os.output("Directory renamed successfully")
```

#### os.mvfile(source, destination)
Moves a file to a new location. Returns `true` on success, `false` on failure.

```apex
import os

result = os.mvfile("doc.txt", "backup/doc.txt")

if result == false
    os.output("Could not move the file")
else
    os.output("File moved successfully")
```

#### os.mvdir(source, destination)
Moves a directory to a new location. Returns `true` on success, `false` on failure.

```apex
import os

result = os.mvdir("project", "archive/project")

if result == false
    os.output("Could not move the directory")
else
    os.output("Directory moved successfully")
```

### Copy
#### os.cpfile(source, destination)
Copies a file. Returns `true` on success, `false` on failure.

```apex
import os

result = os.cpfile("original.txt", "copy.txt")

if result == false
    os.output("Could not copy the file")
else
    os.output("File copied successfully")
```

#### os.cpdir(source, destination)
Recursively copies a directory with all its contents. Returns `true` on success, `false` on failure.

```apex
import os

result = os.cpdir("my_project", "backup/my_project")

if result == false
    os.output("Could not copy the directory")
else
    os.output("Directory copied successfully")
```

### Create and Delete
#### os.mkfile(filename)
Creates an empty file. Returns `true` on success, `false` on failure.

```apex
import os

result = os.mkfile("new_file.txt")

if result == false
    os.output("Could not create the file")
else
    os.output("File created successfully")
```

#### os.mkdir(path)
Creates a new folder. Returns `true` on success, `false` if the folder already exists or can't be created.

```apex
import os

result = os.mkdir("my_project")

if result == false
    os.output("Folder already exists or can't be created")
else
    os.output("Folder created successfully")
```

#### os.rmfile(path)
Deletes a file permanently. Returns `true` if deleted, `false` if the file doesn't exist or can't be deleted.

```apex
import os

result = os.rmfile("temp.txt")

if result == false
    os.output("Could not delete the file")
else
    os.output("File deleted successfully")
```

#### os.rmdir(path)
Removes an empty folder. Returns `true` on success, `false` if the folder isn't empty or doesn't exist. This only works on empty folders — use `os.rmfile()` to delete files inside first.

```apex
import os

result = os.rmdir("old_folder")

if result == false
    os.output("Could not remove the directory")
else
    os.output("Directory removed successfully")
```

### Directory Navigation
#### os.listdir(path)
Returns a table of names — all files and folders inside the given folder. If no path is given, lists the current folder. Returns an empty table on failure.

```apex
import os

items = os.listdir(".")

if items == false
    os.output("Could not list directory contents")
else
    os.output("Contents of current directory:")
    for item in items
        os.output("  {item}")
```

#### os.getcd()
Returns the current directory as a string — where your program is running from. Returns `false` on failure.

```apex
import os

current_folder = os.getcd()

if current_folder == false
    os.output("Could not get current directory")
else
    os.output("Running from: {current_folder}")
```

#### os.setcd(path)
Changes the current directory. Returns `true` on success, `false` on failure.

```apex
import os

result = os.setcd("/home/user/projects")

if result == false
    os.output("Could not change directory")
else
    os.output("Directory changed successfully")
```

### Permissions

#### os.chmod(path, mode)
Changes file permissions. The `mode` is a number (e.g., `755` for rwxr-xr-x on Unix). Returns `true` on success, `false` on failure.

```apex
import os

result = os.chmod("script.sh", 755)

if result == false
    os.output("Could not change permissions")
else
    os.output("Permissions changed successfully")
```

### Environment Variables
#### os.getenv(name)
Gets the value of an environment variable. Returns the value as a string, or `false` if the variable doesn't exist.

```apex
import os

path = os.getenv("PATH")

if path == false
    os.output("Environment variable not found")
else
    os.output("PATH: {path}")
```

#### os.setenv(name, value)
Sets an environment variable. Returns `true` on success, `false` on failure.

```apex
import os

result = os.setenv("MY_VAR", "hello")

if result == false
    os.output("Could not set environment variable")
else
    os.output("MY_VAR set to: {os.getenv("MY_VAR")}")
```

#### os.env()
Returns a table containing all environment variables as key-value pairs. Returns `false` on failure.

```apex
import os

env_vars = os.env()

if env_vars == false
    os.output("Could not get environment variables")
else
    os.output("HOME = {env_vars.HOME}")
    os.output("PATH = {env_vars.PATH}")
```

### System Info
#### os.platform()
Returns a string identifying your operating system, such as `"Windows"`, `"macOS"`, `"iOS"`, `"tvOS"`, `"watchOS"`, `"Android"`, `"Linux"`, `"FreeBSD"`, `"OpenBSD"`, `"NetBSD"`, `"QNX"`, or `"Unix"`. Returns `false` if the platform cannot be detected.

```apex
import os

system = os.platform()

if system == false
    os.output("Could not detect platform")
else
    os.output("You're running on {system}")
```

#### os.architecture()
Returns a string identifying the system's processor architecture, such as `"x86_64"`, `"arm64"`, or `"aarch64"`. Returns `"false"` if the architecture cannot be detected.

```apex
import os

arch = os.architecture()

if name == false
    os.output("Could not get architecture")
else
    os.output("System Architecture: {arch}")
```

#### os.hostname()
Returns the system's hostname as a string. Returns `false` on failure.

```apex
import os

name = os.hostname()

if name == false
    os.output("Could not get hostname")
else
    os.output("Hostname: {name}")
```

#### os.user()
Returns the current user's login name as a string. Returns `false` if it can't be determined.

```apex
import os

username = os.user()

if username == false
    os.output("Could not get username")
else
    os.output("Logged in as: {username}")
```

#### os.homedir()
Returns the current user's home directory path as a string. Returns `false` if it can't be determined.

```apex
import os

home = os.homedir()

if home == false
    os.output("Could not get home directory")
else
    os.output("Home folder: {home}")

    // Example: change to home directory
    result = os.setcd(home)
    if result == false
        os.output("Could not change to home directory")
    else
        os.output("Changed to home directory")
```

#### os.pid()
Returns the current process ID as a number. Returns `false` on failure.

```apex
import os

process_id = os.pid()

if process_id == false
    os.output("Could not get process ID")
else
    os.output("Process ID: {process_id}")
```

### Process Management
#### os.spawn(command)
Starts a new process from a shell command. Returns the PID (process ID) of the new process as a number, or `false` on failure. The program does not wait for the process to finish.

```apex
import os

pid = os.spawn("notepad.exe")    // Windows
// pid = os.spawn("gedit &")     // Linux

if pid == false
    os.output("Could not start process")
else
    os.output("Started process with PID: {pid}")
```

#### os.waitpid(pid)
Waits for a specific process to finish and returns its exit code. Returns a number (the exit code) on success, `false` if the process doesn't exist or can't be waited on.

```apex
import os

pid = os.spawn("my_program")

if pid == false
    os.output("Could not start process")
else
    exit_code = os.waitpid(pid)
    if exit_code == false
        os.output("Could not wait for process")
    else
        os.output("Process exited with code: {exit_code}")
```

#### os.kill(pid)
Terminates the process with the given PID. Returns `true` on success, `false` on failure.

```apex
import os

result = os.kill(12345)

if result == false
    os.output("Could not terminate process")
else
    os.output("Process terminated successfully")
```

#### os.system(command)
Runs a system command as if you typed it in the terminal. Returns the command's exit code. Available commands depend on your operating system. Returns `false` on failure.

```apex
import os

exit_code = os.system("echo Hello from terminal")     // Works on Linux/macOS
// exit_code = os.system("dir")                       // Windows — list files
// exit_code = os.system("ls")                        // Linux/macOS — list files

if exit_code == false
    os.output("Could not execute command")
else
    os.output("Command exited with code: {exit_code}")
```

### Time and Exit
#### os.time()
Returns the current time as a number — seconds since January 1, 1970. Useful for measuring how long something takes. Returns `false` on failure.

```apex
import os

start = os.time()

if start == false
    os.output("Could not get system time")
else
    // ... do something slow ...
    end = os.time()
    elapsed = end - start
    os.output("Took {elapsed} seconds")
```

#### os.wait(seconds)
Pauses the program for the given number of seconds. You can use decimals for fractions of a second. Negative values are treated as `0`. Returns `true` on success, `false` on failure.

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