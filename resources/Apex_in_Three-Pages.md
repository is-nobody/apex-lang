# Apex in Three-Pages
**Minimalist Core Reference (v26.06)**

### 1. Variables & Data Types
A variable's type is set at creation and **cannot change**.
```apex
x = 2024        // number (whole)
x = 3.14        // number (decimal, always use dot)
x = "Hello"     // string (always double quotes)
x = true        // boolean (lowercase)
x = false       // boolean (lowercase)
x = ()          // table (empty container)
```
- **Comments:** `//` ignores the rest of the line.
- **Type Conversion:** `number("42")` → `42`, `string(42)` → `"42"`. Fails on invalid input.

### 2. Operators
**Arithmetic** (work only on numbers)
| Op | Name | Example (`x=10, y=3`) | Result |
|----|------|-----------------------|--------|
| `+` | Add | `x + y` | `13` |
| `-` | Sub | `x - y` | `7` |
| `*` | Mul | `x * y` | `30` |
| `/` | Div | `x / y` | `3.333..` |
| `%` | Mod | `x % y` | `1` |
*Precedence:* `()`, then `* / %`, then `+ -`.

**Comparison** (return `boolean`)
| Op | Name | Example | Result |
|----|------|---------|--------|
| `==` | Equal | `5 == 5` | `true` |
| `!=` | Not equal | `5 != 3` | `true` |
| `<` | Less | `3 < 5` | `true` |
| `>` | Greater | `5 > 3` | `true` |
| `<=` | Less/Eq | `3 <= 3` | `true` |
| `>=` | Great/Eq | `5 >= 5` | `true` |
- Strings only support `==` and `!=`.
- Precedence: lower than arithmetic.

**Logical** (combine booleans)
| Op | Name | Example | Result |
|----|------|---------|--------|
| `and` | AND | `true and false` | `false` |
| `or` | OR | `true or false` | `true` |
| `not` | NOT | `not true` | `false` |
*Full Precedence:* `()` → `* / %` → `+ -` → `< > <= >=` → `== !=` → `not` → `and` → `or`.

### 3. Strings Deep Dive
- **Interpolation:** Embed variables or expressions inside `"..."` using `{}`.
  ```apex
  name = "Alice"
  os.output("Hello, {name}")          // "Hello, Alice"
  os.output("Count: {5 * 2}")         // "Count: 10"
  ```
- **Double Quotes Inside:** No escaping. Write `"` directly inside `"..."`.
  ```apex
  quote = "He said: "I hate donuts!""
  ```
- **Multiline:** Press Enter inside the quotes.
  ```apex
  text = "Line one
  Line two"
  ```

### 4. Tables Deep Dive
Tables are universal containers `()`. They are ordered lists **and** key-value maps. Ordered items use numbers, keys use names. You can mix them.
```apex
// Ordered list (indices start at 1)
colors = ("red", "green", "blue")
first = colors.1       // "red"

// Key-value pairs (keys without quotes)
user = (name = "Alice", age = 30)
user.age = 31           // Update existing
user.city = "Dubai"     // Add new

// Mixed table (ordered items FIRST, then key-value)
person = ("Alice", "Manager", department = "Engineering", years = 5)
name = person.1               // "Alice"
dept = person.department      // "Engineering"
```
- **Nested Tables:** Tables can hold other tables. Access via chain: `company.address.city`.
- *Accessing non-existent keys or index 0 throws an error.*

### 5. Control Flow
**Block structure:** If line 1 starts with `function`, `if`, `elif`, `else`, `for`, `while`, line 2 **must be indented with 4 spaces**.
```apex
if condition
    // 4 spaces indent
elif other_condition
    // 4 spaces indent
else
    // 4 spaces indent
```
**If Statement:**
```apex
score = 85
if score >= 90
    os.output("A")
elif score >= 80
    os.output("B")    // This runs, then skips rest
else
    os.output("F")
```

**For Loop:** `for var = start, end [, step]`. Runs **including** `end` value.
```apex
for i = 1, 5           // 1, 2, 3, 4, 5
for i = 0, 10, 2       // 0, 2, 4, 6, 8, 10
for i = 5, 1, -1       // 5, 4, 3, 2, 1
```

**While Loop:** Runs as long as condition is `true`. Ensure loop changes condition to avoid infinite loop.
```apex
counter = 1
while counter <= 5
    os.output(counter)
    counter = counter + 1
```

**Break & Continue:** Work in both `for` and `while`.
- `break`: Exits the loop immediately.
- `continue`: Skips the rest of the current iteration, jumps to the next check.

### 6. Functions
**Declaration & Call:**
```apex
function add(a, b)      // a, b are parameters
    return a + b        // return value to caller

result = add(5, 3)      // call, result is 8
```
- **No return:** Function automatically returns `false`.
- **`return` exits the function instantly**, nothing after it runs.
- **Call order:** Can nest calls `add(5, multiply(2,3))` or chain them.

### 7. Imports
`import` paths are **always relative to the main file** (the one you run).
```apex
import os                // built-in
import database          // file 'database.apex' in same folder
import utils.math        // file 'utils/math.apex'
```
- A dot `.` means "go inside this folder".
- Even when importing inside a nested file, write path from the main file's perspective.