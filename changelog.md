# Apex 26.07 (31 July, 2026)
**Initial public release of the Apex programming language.**

## Language
- **Types:** `number`, `string`, `boolean`, `none`, `table`, `function`
- **Indentation-based blocks** (4 spaces)
- **Explicit boolean conditions** — no truthy/falsy
- **Mutable typing:** variables can change type on reassignment
- **Strict table keys:** numeric and string keys are distinct, no auto-conversion
- **Table iteration returns values,** not indices
- **String, table and function equality** (`==`, `!=`), including nested/recursive tables
- **Ternary expressions:** `value if condition else value`
- **Control flow:** `if`/`elif`/`else`, `for` (range, table, condition-based), `break`/`continue`/`return`
- **First-class functions** with closures
- **`none`** for absent values, missing keys, void returns
- **String concatenation** via interpolation only

## Syntax
- Full UTF-8: variable names, strings, errors
- Single- and double-quoted strings with escapes
- String interpolation: `"Hello {name}"`
- Shebang support: `#!`
- Resilient parsing: multiple errors per pass
- Stack overflow detection (max 8192 call frames)
- Unreachable code detection

## Built-in Modules
`os`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `codecs`

Key highlights:
- `sys.time()`, `sys.date()` → table with `year`, `month`, `day`, `hour`, etc.
- `string.length()` and `string.sub()` work on characters, not bytes
- `string.isletter()`, `string.isnumber()` — full Unicode
- `string()` supports tables
- `ffi` — C library loading and memory management
- `codecs` — Base64, JSON, CSV, XML

## Tooling
- Cross-platform: Windows, Linux, macOS
- REPL with error context
- `apex build <file>` with auto-detected stub

## Performance
- NaN-boxing: all values in 64 bits
- Persistent string interning
- Local variable optimization
- Fast paths for 0–2 argument functions
- Lazy table allocation: empty tables cost O(1) with zero allocations