# Apex 26.07 (31 July, 2026)
**Second release of the Apex programming language.**

## Behavior Changes
- `none` type: a new data type replacing `false` to represent the absence of a value.
- Built-in modules (`os`, `sys`, `string`, `random`, `math`, `ffi`, `codecs`) now return `none` instead of `false` on errors.
- Accessing a missing table key now returns `none`.
- Functions without an explicit `return` now return `none`.
- Variables can now change type on reassignment.
- Strict table key typing: numeric keys and string keys are now distinct and separate.
- Tables no longer perform automatic conversion between numbers and strings for lookups or assignments.
- Local variable optimization: variables preferentially compiled as local registers; globals reserved for module exports.
- Renamed `string.len` to `string.length` for clarity and consistency.
- Moved `os.time()` to `sys.time()` for better logical organization: time is a system property, not an OS operation.
- Internal value representation switched to NaN-boxing: all runtime values (numbers, strings, tables, booleans, none, functions) are stored in a single 64-bit field using IEEE 754 NaN space for type tagging, reducing memory usage and improving cache locality.
- String interning now persistent: all interned strings are retained for the lifetime of the VM, eliminating use-after-free bugs and improving string comparison performance.
- Removed string interning limit: previously, string interning was capped at 50,000 entries, after which strings were allocated individually, causing memory corruption on deallocation.
- `apex build <file>` without explicit os and architecture now uses the current running executable as the stub template instead of searching for a separate stub file by name.

## New Features
- Multi-language support: variable names, string literals, and error reporting fully support UTF-8.
- Code can be written in Cyrillic, Arabic, Chinese, Japanese, etc.
- UTF-8 string operations: `string.length()` and `string.sub()` work with characters, not bytes.
- Full Unicode support for all scripts and emoji.
- `string()` now supports tables.
- Shebang support: scripts can start with `#!`.
- Indentation-based folding in the VS Code extension.
- Resilient tokenization and parsing: multiple errors reported in a single pass.
- Improved error messages: entire expression highlighted on arithmetic type errors.
- Stack overflow detection with runtime error messages.
- Increased max call frames from 1024 to 8192.
- Single-quoted strings: use double or single quotes with full escape sequence support.
- Ternary expressions: one-line conditional expressions (`value if condition == true else value`).
- Function equality comparison: `==` and `!=` operators now support comparing function values by reference.
- Improved `for` loop errors: missing end/step values now report clear messages instead of generic or misleading errors.
- Removed `random.secure_token_bytes` function.
- New string functions: `string.isletter()` and `string.isnumber()` with full Unicode support for all scripts (Latin, Cyrillic, Arabic, CJK, Devanagari, and more).

## Bug Fixes
- Sparse numeric keys: correct removal with holes, accurate size calculation.
- Fall through to hash lookup when numeric key not found in array part of table.
- Column offset corrected for expressions inside string interpolation.
- Removed redundant "Variable already declared in this scope" and "Assignment to undefined variable" errors.
- Enforced explicit boolean conditions in `if` and `for` with proper validation of `and`/`or`.
- Eliminated duplicate semantic errors in string interpolation via error position history buffer.
- Condition-based `for` loops: support arbitrary boolean expressions; fixed double-free crash.
- Fixed false `break`/`continue` errors in nested blocks.
- Trailing comma in function parameters: fixed segmentation fault with null-checks.
- Unreachable code detection: compile-time error for statements following `return`, `break`, or `continue`.
- String concatenation enforcement: `+` with string literal and `any`-typed variable now raises parse error.
- `sys.environment()` no longer mutates original environment strings; fixes crashes on read-only memory.
- Normalized floating-point zero hashing: `-0.0` and `+0.0` no longer occupy different hash buckets.
- Fixed potential memory leak in intern table cleanup.
- Pooled tables properly reset capacity after returning to pool, preventing oversized reallocations.
- Fixed segmentation fault in table assignment when table register contains non-table value: added type check.
- Prevent segmentation fault on invalid expression syntax (e.g., `x = * 5` or `x = + 5`): parser now correctly handles failed prefix expression parsing and returns a proper error instead of crashing.
- Simplified string() and number(): unsupported types now consistently return none via default case instead of explicit per-type handling
- Fixed segmentation fault in parser on binary operator with missing right operand (e.g., `or < c`): added null-check for failed expression parsing.
- Fixed error highlight length for undefined variables in string interpolation: now correctly includes braces `{var}` in the underline.

---

# Apex 26.06 (30 June, 2026)
**First release of the Apex programming language.**

## Core Language Features
- Indentation-based blocks: 4-space indentation required for code blocks.
- Explicit boolean conditions: all `if` and `for` conditions must be explicit boolean expressions, no truthy/falsy values.
- Static typing enforcement: variable types cannot change after declaration.
- Data types: `number` (64-bit floating-point doubles), `string` (UTF-8 with interpolation), `boolean` (`true` or `false`), `table` (universal container for ordered arrays and key-value dictionaries), `function` (first-class with closures).
- Dynamic scoping for variables.
- Type inference at declaration, enforced thereafter.
- Constant optimization: numeric literals evaluated at compile-time.
- Logical operators: `and`, `or`, `not` with boolean operands only.
- String concatenation via interpolation only, no `+` for strings.
- Control flow: `if`/`elif`/`else` multi-branch, `for` loops (range, table iteration, condition-based), `break`/`continue`, `return`.
- First-class functions: assign to variables, pass as arguments, nested functions.
- Built-in functions: `number()`, `string()`, `type()`.
- Mixed tables: positional items `[1, 2, 3]` and key-value pairs `[name = "Alice"]`, nested tables.
- String interpolation: embedded expressions `"Value: {x + y}"`, escape sequences `\{` and `\}`, multiline strings.
- Module system: `import module.path` syntax, separate namespaces, user modules via dot notation, relative paths relative to main file.
- Built-in modules: `os`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `codecs`.

## Built-in Libraries
- `os`: I/O, time, process control, file/directory operations, permissions.
- `sys`: platform info, runtime info, system resources, terminal detection.
- `math`: constants, rounding, powers/roots, logarithms, trigonometry, number theory.
- `string`: length, case conversion, substring, search/replace, split/join, trimming.
- `table`: manipulation, clear, copy, merge operations.
- `ffi`: C library loading, function calls, memory management.
- `random`: basic generation, seeding, sequence operations, distributions, secure randomness.
- `codecs`: Base64, JSON, CSV, XML encoding/decoding.

## Development Tools
- Cross-platform builds: Windows, Linux, macOS.
- REPL: interactive development with error context and module support.
- Error handling: compile-time type/undefined/module checks, runtime failure with `false`, line/column context in messages.