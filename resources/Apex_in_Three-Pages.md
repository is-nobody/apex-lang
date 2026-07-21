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
- **Scope:** Global by default. Use `function` blocks for local scope.

## Operators & Precedence
| Level | Ops | Notes |
|-------|-----|-------|
| 1 | `()` | Grouping, function calls |
| 2 | `+ - * / %` | Arithmetic (only numbers) |
| 3 | `< > <= >=` | Comparison (numbers only) |
| 4 | `== !=` | Equality (all types except tables) |
| 5 | `not` | Logical negation |
| 6 | `and` | Short-circuit AND |
| 7 | `or` | Short-circuit OR |

- **Strings:** Only support `==` and `!=`.
- **Tables:** Cannot be compared with `==` or `!=`.

## Strings & Interpolation
- **Interpolation:** `"Hello {name}, result: {5 * 2}"`. Expressions inside `{}` are evaluated.
- **Escapes:** `\"` (quote), `\\` (backslash), `\{` (literal brace), `\n` (newline), `\t` (tab).
- **Multiline:** Supported natively within quotes.

## Tables `[]`
Unified structure for arrays (1-based) and maps. **No dot access for tables.**

```apex
// Mixed definition: positional items first, then key-value pairs
data = ["item1", "item2", key1 = "val1", key2 = 100]

// Access
x = data[1]           // Positional (1-based index)
y = data["key1"]      // Key access (bracket notation ONLY)

// Mutation
data["key1"] = "new"  // Update
data["new_key"] = 50  // Add
```

- **Nested:** `user["address"]["city"]`.
- **Utilities:** `table.size(t)`, `table.keys(t)`, `table.has(t, "k")`, `table.remove(t, index)`.

## Control Flow
**Indentation:** Blocks after `if`, `for`, `function`, etc. require 4 spaces exactly.

**If/Elif/Else:**
```apex
if x == 10
    os.output("Ten")
elif x > 10
    os.output("More")
else
    os.output("Less")
```

**Ternary:**
```apex
result = "Pass" if score >= 50 else "Fail"
```

Single condition only. For 2+ checks, use `if-elif-else`.

**For Loop (Range):**

```apex
for i = 1, 5        // 1, 2, 3, 4, 5
for i = 0, 10, 2    // Explicit step
```

**For Loop (Condition-based):**

```apex
for running == true
    os.output("Still running...")
```

**For Loop (Table Iteration):**

```apex
for v = my_table    // Iterates over values
    os.output(v)
```

## Functions

```apex
function calculate(a, b)
    return a + b

result = calculate(5, 10)
```

- **Return:** Defaults to `false` if omitted.
- **Recursion:** Supported up to depth 512.
- **Built-ins:** Organized in modules (`os.`, `math.`, `string.`, `table.`).

## Modules & Imports
Paths are relative to the **entry point** file.

```apex
import os             // Built-in
import utils.helper   // Loads 'utils/helper.apex'
```

- **Access:** `helper.my_func()`.
- **Dot Restriction:** Dot access (`mod.func`) is reserved **strictly** for imported modules. Use brackets `t["key"]` for table keys.