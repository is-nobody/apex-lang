# Apex 26.10 (October 31, 2026)
## Security
- **Tokenizer:** Prevented heap buffer overflow in `read_number` on numeric literals longer than 63 digits.
- **`.apexc` loader:** Bounds-checked every section count and `malloc` result so malformed or truncated bytecode is rejected instead of corrupting memory.
- **Standalone binaries:** Embedded bytecode temp file is now claimed atomically (`mkstemp` on POSIX, `CreateFileA(CREATE_NEW)` with `FILE_FLAG_OPEN_REPARSE_POINT` on Windows), closing the symlink and TOCTOU windows opened by the previous predictable PID-based path.
- **`os.read`:** Non-seekable and zero-size streams (pipes, `/proc`) are now read in chunks instead of trusting `ftell`, eliminating a heap overflow on `-1`/`0`.
- **`crypto`:** `compare_strings` folds the length difference into the accumulator so mismatched lengths no longer leak through timing, and every secure random primitive fails closed (`none`) when the OS CSPRNG is unavailable instead of falling back to `srand(time ^ clock)`.
- **`regex` / `json` / `xml`:** Recursive walks now enforce a step budget and depth limit, so adversarial patterns and deeply nested inputs return `none` instead of hanging a worker or overflowing the C stack.
- **`datetime`:** Year range is enforced before any `(long long)` cast, non-integer fields are rejected, `add` offsets and `from_timestamp` inputs are bounded, and out-of-range results return `none`.

## Runtime
- **Table correctness:** `for value in table` detects structural mutation mid-loop via a per-table generation counter and exits cleanly (fixes a use-after-free), and `==` on tables uses pair-stack cycle detection so structurally-different cyclic tables no longer compare equal.
- **Globals:** The VM's globals array is now dynamically sized (up to a 16M sanity ceiling) instead of a fixed 512-slot inline array, and every slot is reset to `none` before each `apex_run_string`, so large projects and repeated runs no longer corrupt or leak state.
- **JIT:** Loops touching globals with index ≥ 64 are correctly guarded by a per-loop `uses_globals` flag, fixing a silent miscompile.
- **Scheduler:** `pending_workers` is now `_Atomic int`, removing a C11 data race with the unlocked hint reads in the timer wait path.

## Compiler
- **Parser:** The builtin function hash table is now built under a one-time initializer (`pthread_once`/`InitOnceExecuteOnce`), removing a data race when two host threads parse concurrently.
- **Codegen:** Transient statement lists built by `emit_unswitched_for` are freed after each use, removing a per-loop leak.

## Libraries
- **`random`:** All `srand`/`rand` access is serialised under a process-wide once-initialised mutex, so two `ApexState`s in different host threads no longer race inside libc's PRNG.
- **`os.access`:** The mode argument is now interpreted as octal digits (`755` → `rwxr-xr-x`), matching `chmod` conventions. Digits `8`/`9` and negatives return `false`.

## Tooling
- **REPL:** Left and right arrow keys navigate the current input line.
- **Embedding C API:** Hardened for multi-state host applications. `stack_grow` and `apex_check_stack` now handle OOM and integer overflow safely; every public push function surfaces failure through the new `apex_had_error`/`apex_clear_error` accessors instead of silently dropping values; `apex_run_string` resets VM globals before each run so state cannot leak between runs; and the builtin hash table is built exactly once process-wide. Also updated the C API for embedding Apex in host applications.

## Modules & Scope
- Fixed top-level reassignments to mirror into global slots.

# Apex 26.09 (September 30, 2026)
**Initial public release of the Apex programming language.**

## Language
- **Five data types:** `number`, `string`, `boolean`, `none`, `table`
- **Indentation-based blocks** (4 spaces)
- **Explicit boolean conditions** — no truthy/falsy
- **Mutable typing:** variables can change type on reassignment
- **Strict table keys:** numeric and string keys are distinct, no auto-conversion
- **Table iteration returns values,** not indices
- **String, table and function equality** (`==`, `!=`), including nested/recursive tables
- **Ternary expressions:** `value if condition else value`
- **Control flow:** `if`/`else if`/`else`, `for` (range, table, condition-based), `break`/`continue`/`return`
- **`match`/`case`** for constant-pattern branching with optional default
- **`none`** for absent values, missing keys, void returns
- **String concatenation** via interpolation only
- **Constants:** `constant NAME = value` for immutability
- **Functions** with fixed arity, single return value, early return
- **Recursion** with tail-call optimization
- **1-indexed** tables and strings
- **Errors as values** — no exceptions, no `try`/`catch`

## Syntax
- Full UTF-8: variable names, strings, errors
- Single- and double-quoted strings with escapes
- Escape sequences: `\n`, `\t`, `\r`, `\"`, `\'`, `\\`, `\{`, `\}`, `\NNN` (octal)
- Multiline strings preserving source indentation
- String interpolation: `"Hello {name}"`
- Interpolation supports expressions: `"Total: {count * 2}"`
- Shebang support: `#!`
- Line comments with `//`
- Scientific notation: `1.5e8`, `9.1e-31`, `2E+5`
- Resilient parsing: multiple errors per pass
- Stack overflow detection (max 1024 call frames)
- Unreachable code detection
- Unexpected indentation detection
- Type checking: arithmetic, comparisons, logical operators, function arguments
- Module file validation and import resolution
- Nested block depth limits (512 functions, 512 loops)

## Operators
- **Arithmetic:** `+`, `-`, `*`, `/`, `%`
- **Comparison:** `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logical:** `and`, `or`
- **IEEE 754 behavior** for division by zero: `inf`, `-inf`, `nan`, `-nan`
- **Operator precedence:** `()` > `*`/`/`/`%` > `+`/`-` > comparisons > `==`/`!=` > `and` > `or`

## Built-in Modules
`os`, `sys`, `math`, `string`, `table`, `random`, `json`, `xml`, `csv`, `base`, `regex`, `crypto`, `zip`, `datetime`.

- **`os`** — filesystem, processes, stdin/stdout, `os.args()`
- **`sys`** — platform info, environment, disk, terminal detection
- **`math`** — trigonometric, logarithmic, rounding, GCD, factorial, NaN/Inf checks
- **`string`** — length, case, slice, split, join, trim, find, replace, repeat
- **`table`** — size, has, remove, keys, values, clear, copy, merge
- **`random`** — float, integer, choice, shuffle, sample, normal, triangular, expovariate, betavariate
- **`json`** — encode/decode with UTF-8, surrogate pairs, nested tables
- **`xml`** — encode/decode with attributes and nested elements
- **`csv`** — RFC 4180 encode/decode with quoted fields
- **`base`** — Base16, Base32, Base32Hex, Base62, Base64, Base64URL, Base85
- **`regex`** — search, find_all, replace, split with `ignorecase` and `dotall`
- **`crypto`** — MD5, SHA-1, SHA-256, SHA-384, SHA-512, HMAC, PBKDF2, AES-128/192/256-CBC, secure random
- **`zip`** — pack/unpack with CRC32 and DOS timestamps
- **`datetime`** — now, local, timestamp, parse, format, add, diff

## Built-in Functions
- `type(value)` — returns the type name as a string
- `number(value)` — converts to number, `none` on failure
- `string(value)` — converts to string representation

## Imports
- Entire file: `import os`, `import utils/math.apex`
- Subfolders: `import utils/math.apex`
- Aliasing with `as`: `import utils/calc.apex as calc`
- Paths always relative to the main file

## Modules & Scope
- Per-function local scope with pre-declared slots
- Module globals via dotted names (`module.name`)
- Constants propagate compile-time constant values
- Nested functions capture enclosing scope

## Async / Await
- `async function` returns a future without blocking
- `await` starts the body and returns its result
- `await` allowed in async functions and at program top level
- Scheduler drives coroutines, timers, and background workers

## Tooling
- Cross-platform: Windows, Linux, macOS
- REPL with error context and raw terminal input
- CLI commands:
  - `apex <file.apex>` — run a script
  - `apex version` — print version, compiler, platform
  - `apex build <file.apex>` — bundle into standalone executable
  - `apex build <os> <arch> <file.apex>` — cross-build
  - `apex compile <file.apex>` — compile to `.apexc` bytecode
  - `apex emit <file.apex>` — disassemble bytecode
  - `apex jit on <file.apex>` — run with JIT
  - `apex jit off <file.apex>` — run without JIT
- Standalone binaries embed bytecode after a marker, no runtime dependency
- Colored error output with source line highlighting and underlines

## Performance
- NaN-boxing: all values in 64 bits
- Register-based VM with computed-goto dispatch
- x86-64 JIT compiler (optional, enabled by default when built on x86-64)
- JIT specializes numeric-pure functions and loops, with integer-only fast paths
- Persistent string interning
- Dual array/hash tables: O(1) integer indexing plus O(1) string keys
- Lazy table allocation: empty tables cost O(1) with zero allocations
- Local variable optimization and register reuse via linear scan
- Fast paths for 0–2 argument functions (`CALL_0`, `CALL_1`, `CALL_2`)
- Compile-time optimizations: constant folding, dead-code elimination, peephole fusion, LICM, loop unswitching, loop unrolling, value numbering, function inlining, tail-call optimization, IV strength reduction, symbolic loop folding

## Compiler
- Single-pass tokenizer with indent/dedent emission
- Recursive-descent Pratt parser
- Rich AST with per-node source positions
- Multi-error resilience with error history
- Bytecode compiler with optimizer pipeline

## Platforms
- **Windows** x86-64
- **macOS** x86-64
- **Linux** x86-64
- Cross-build stubs supported for `windows`, `linux`, `macos` on `x86-64` and `arm64`

## VS Code Extension
- Syntax highlighting for the full language
- Auto-closing brackets and quotes
- Indentation rules for `function`, `if`, `else if`, `else`, `for`, `match`, `case`
- Autocomplete for keywords, modules, and ~200 library functions
- Hover documentation for keywords and libraries
- Outline view for functions
- `Apex: Run Current File` command and `F5` shortcut