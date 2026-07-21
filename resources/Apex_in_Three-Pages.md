# Apex in Three Pages (26.07)
## Types & Variables
Dynamic typing: type can change on reassignment.

```apex
x = 42          // number (int/float unified)
x = "Apex"      // string (double quotes only)
x = true        // boolean
x = none        // none (intentional absence)
x = []          // table (universal container)
```

- **Conversion:** `number("10")`, `string(10)`. Returns `none` on failure.
- **Scope:** `function` and `for` create new scopes, top-level variables are optimized as locals, `if`/`elif`/`else` don't create scope.

## Operators & Precedence
| Level |     Ops     |           Notes           |
|-------|-------------|---------------------------|
|   1   | `()`        | Grouping, function calls  |
|   2   | `+ - * / %` | Arithmetic (only numbers) |
|   3   | `< > <= >=` | Comparison (numbers only) |
|   4   | `== !=`     | Equality (all types)      |
|   5   | `not`       | Logical NOT               |
|   6   | `and`       | Logical AND               |
|   7   | `or`        | Logical OR                |

## Strings & Interpolation
- **Interpolation:** `"Hello {name}, result: {5 * 2}"`. Expressions inside `{}` are evaluated.
- **Escapes:** `\"` (quote), `\\` (backslash), `\{` (literal brace), `\n` (newline), `\t` (tab).
- **Multiline:** Supported natively within quotes.

## Tables `[]`
Unified structure for keys and values (1-based).

```apex
// mixed definition: positional items first, then key-value pairs
data = ["key", "value" = 96]

// access
x = data[1]           // positional (1-based index)
y = data["key"]       // key access (bracket notation only)

// mutation
data["key"] = "new"   // update
data["new_key"] = 69  // add
```

- **Nested:** `user["address"]["city"]`.
- **Utilities:** `table.size(t)`, `table.keys(t)`, `table.has(t, "k")`, `table.remove(t, index)`.

## Control Flow
**Indentation:** Next line after `if`/`elif`/`else`, `for`, `function` require 4 spaces exactly.

**If/Elif/Else:**
```apex
import os
x = 15
if x == 10
    os.output("Ten")
elif x > 10
    os.output("More")
else
    os.output("Less")
```

**Ternary:**
```apex
import os
score = 20
result = "Pass" if score >= 50 else "Fail"
os.output(result)
```

Single condition only. For 2+ checks, use `if-elif-else`.

**For Loop Counter:**

```apex
import os
for i = 1, 5, 2
    os.output(i)
```

`for i = 1, 5` sets a loop from 1 to 5 with a default step of 1, but the third parameter (the step) overrides it with a value of 2.

**For Loop Condition:**

```apex
import os
running = true
for running == true
    os.output("running...")
```

**For Loop Table:**

```apex
import os
table = ["apple", "banana", "cherry"]
for fruit = table    // iterates over values
    os.output(fruit)
```

## Functions

```apex
function calculate(a, b)
    return a + b

result = calculate(5, 10)
```

- **Return:** Defaults to `none` if omitted.
- **Recursion:** Supported up to depth 8192.
- **Built-ins:** Organized in modules (`os`, `math`, `string`, `table`, `sys`, `ffi`, `random`, `codecs`).

## Modules & Imports
Paths are relative to the **entry point** file.

```apex
import os             // built-in
import utils.helper   // loads 'utils/helper.apex'
```

- **Access:** `helper.my_func()`.
- **Dot Restriction:** Dot access (`mod.func`) is reserved **strictly** for imported modules. Use brackets `t["key"]` for table keys.