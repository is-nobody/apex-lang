# Apex in Three Pages (26.06)
## Types & Variables
Strict typing: type is fixed at initialization. No implicit coercion.
```apex
x = 42          // number (int/float unified)
x = "Apex"      // string (double quotes only)
x = true        // boolean
x = []          // table (universal container)
```
- **Conversion:** `number("10")`, `string(10)`. Returns `false` on failure.
- **Scope:** Global by default. Use `function` blocks for local scope.

## Operators & Precedence
| Level | Ops | Notes |
|-------|-----|-------|
| 1 | `()` | Grouping, function calls |
| 2 | `* / %` | Arithmetic |
| 3 | `+ -` | Arithmetic (no string concat) |
| 4 | `< > <= >=` | Comparison (numbers only) |
| 5 | `== !=` | Equality (all types) |
| 6 | `not` | Logical negation |
| 7 | `and` | Short-circuit AND |
| 8 | `or` | Short-circuit OR |

- **Strings:** Only support `==` and `!=`.
- **Tables:** Support `==` (reference equality).

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
table.append(data, "item3") // Add positional item
```
- **Nested:** `user["address"]["city"]`.
- **Utilities:** `table.size(t)`, `table.keys(t)`, `table.has(t, "k")`.

## Control Flow
**Indentation:** 4 spaces required for blocks after `if`, `for`, `function`, etc.

**If/Elif/Else:**
```apex
if x == 10
    os.output("Ten")
elif x > 10
    os.output("More")
else
    os.output("Less")
```

**For Loop (Range):**
```apex
// Auto-step detection: step is 1 if start <= end, -1 otherwise
for i = 1, 5        // 1, 2, 3, 4, 5
for i = 5, 1        // 5, 4, 3, 2, 1 (Auto -1)
for i = 0, 10, 2    // Explicit step
```

**For Loop (Condition-based):**
```apex
// Replaces while loop. Requires explicit boolean condition
for running == true
    os.output("Still running...")
```

**For Loop (Table Iteration):**
```apex
for k = my_table    // Iterates over keys
    os.output("{k} = {my_table[k]}")
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