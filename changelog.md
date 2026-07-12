# Apex 26.07 (07.31.2026)
**Second release of the Apex programming language.**

## Behavior Changes
- **`none` type** — a new data type replacing `false` to represent the absence of a value. All built-in modules (`os`, `sys`, `string`, `random`, `math`, `ffi`, `codecs`) now return `none` instead of `false` on errors.
- Accessing a missing table key now returns `none`
- Functions without an explicit `return` now return `none`
- Variables can now change type on reassignment

## New Features
- **Multi-language support** — variable names, string literals, and error reporting now fully support UTF-8, allowing code to be written in any language (Cyrillic, Arabic, Chinese, Japanese, etc.).
- **Table to string conversion** — `string()` now supports tables.
- **Shebang support** — scripts can start with `#!`.
- **Indentation-based folding** in the VS Code extension.
- **Resilient tokenization and parsing** — tokenization and parsing continue after the first error, reporting multiple errors in a single pass.
- **Improved error messages** — the entire expression is now highlighted on arithmetic type errors.
- **Stack overflow detection** — runtime error messages for stack overflow.
- **Increased max call frames** from 1024 to 8192.

## Bug Fixes
- **Sparse numeric keys**: correct removal with holes, accurate size calculation
- **Fall through to hash lookup** when a numeric key is not found in the array part of a table
- **Column offset** corrected for expressions inside string interpolation

---

# Apex 26.06 (06.30.2026)
**First release of the Apex programming language.**

## Core Language Features
### Syntax & Structure
- **Indentation-based blocks**: 4-space indentation required for code blocks
- **No curly braces or end keywords**: Clean, indent-based syntax
- **Explicit boolean conditions**: All `if` and `for` conditions must be explicit boolean expressions (no truthy/falsy values)
- **Static typing enforcement**: Variable types cannot change after declaration

### Data Types
- **number**: 64-bit floating-point (doubles)
- **string**: UTF-8 with interpolation (`"Hello {name}"`)
- **boolean**: `true` or `false`
- **table**: Universal container — ordered arrays and key-value dictionaries
- **function**: First-class with closures

### Variables & Scope
- **Dynamic scoping**
- **Type inference** at declaration, enforced thereafter
- **Constant optimization**: Numeric literals evaluated at compile-time

### Operators
- **Logical**: `and`, `or`, `not` (boolean operands only)
- **String concatenation**: Via interpolation only (no `+` for strings)

### Control Flow
- **If/Elif/Else**: Multi-branch with explicit boolean conditions
- **For loops**: Three variants:
  - Range: `for i = start, end [, step]`
  - Table iteration: `for key = table`
  - Condition-based: `for condition == true`
- **Break/Continue**
- **Return**

### Functions
- **First-class**: Assign to variables, pass as arguments
- **Nested functions**
- **Built-in**: `number()`, `string()`, `type()`

### Tables
- **Mixed**: Positional items `[1, 2, 3]` and key-value pairs `[name = "Alice"]`
- **Nested tables**

### String Interpolation
- **Embedded expressions**: `"Value: {x + y}"`
- **Escape sequences**: `\{` and `\}` for literal braces
- **Multiline strings**

### Modules & Imports
- **Import syntax**: `import module.path`
- **Module isolation**: Separate namespaces
- **Built-in modules**: os, sys, math, string, table, ffi, random, codecs
- **User modules**: Subdirectories via dot notation
- **Relative paths**: Relative to main file

## Built-in Libraries
### OS (`os`)
- I/O, time, process control, file/directory operations, permissions

### System (`sys`)
- Platform info, runtime info, system resources, terminal detection

### Math (`math`)
- Constants, rounding, powers/roots, logarithms, trigonometry, number theory

### String (`string`)
- Length, case conversion, substring, search/replace, split/join, trimming

### Table (`table`)
- Manipulation, operations (clear, copy, merge)

### FFI (`ffi`)
- C library loading, function calls, memory management

### Random (`random`)
- Basic generation, seeding, sequence operations, distributions, secure randomness

### Codecs (`codecs`)
- Base64, JSON, CSV, XML encoding/decoding

## Development Tools
- **Cross-platform builds**: Windows, Linux, macOS
- **REPL**: Interactive development with error context and module support
- **Error handling**: Compile-time type/undefined/module checks; runtime failure with `false`; line/column context in messages