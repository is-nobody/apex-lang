# Apex 26.10 (October 31, 2026)
**Second public release of the Apex programming language.**

## Security
- **Tokenizer:** Fixed heap buffer overflow in `read_number` for literals longer than 63 digits.
- **Tokenizer:** `realloc` results in `add_token`, `read_string`, and `read_identifier` are now checked for NULL before overwriting the live pointer. OOM reports a tokenizer error instead of dereferencing NULL.
- **Parser / Codegen:** Dotted-name assembly no longer overflows `parts[32]`, `full_name[512]`, or `func_name[256]`. Segments are bounded; buffers are written through cursors instead of unbounded `strcat`/`strcpy`.
- **Parser:** Added `MAX_EXPR_DEPTH` (256) cap on expression nesting, so deeply nested parens, tables, unary chains, awaits, and interpolations produce a parse error instead of crashing via C stack exhaustion.
- **Parser:** Added `MAX_CHAIN_DEPTH` (512) cap on iterative chains (`1+1+1+...`, `a.b.c...`, `if/else if/...`) that build deep ASTs without growing `expr_depth`.
- **`.apexc` loader:** Every section count and `malloc` result is bounds-checked; malformed bytecode is rejected.
- **Standalone binaries:** Embedded bytecode temp file is now claimed atomically — `mkstemp` on POSIX, `CreateFileA(CREATE_NEW)` with `FILE_FLAG_OPEN_REPARSE_POINT` on Windows. Closes the symlink/TOCTOU window and removes the 16-bit `GetTempFileNameA` namespace limit.
- **Temp-file cleanup:** `platform_delete_temp_file` no longer falls back to recursive directory deletion on unlink failure; it removes only the exact file it was given.
- **`os.read`:** Non-seekable and zero-size streams are read in chunks instead of trusting `ftell`, closing a heap overflow on `-1`/`0`.
- **`crypto`:** `compare_strings` no longer leaks length through timing; secure random primitives fail closed (`none`) when the OS CSPRNG is unavailable instead of falling back to `srand`.
- **`regex` / `json` / `xml`:** Recursive walks enforce a step budget and depth limit; adversarial input returns `none` instead of hanging or overflowing the C stack.
- **`base.decode_85`:** Added an exact-size decoder. `z` expands to 4 bytes per char, so the old `input_len * 2 + 1` allocation overflowed on any `z`-containing input.
- **`zip.unpack`:** Rejects archives smaller than 22 bytes before computing the EOCD offset. The old `zip_size - 22` underflowed to `SIZE_MAX` and read past the buffer.
- **`datetime`:** Year range enforced before casts; non-integer fields and out-of-range offsets/timestamps are rejected.

## Runtime
- **Table correctness:** `for value in table` detects structural mutation mid-loop and exits cleanly; `==` on tables uses pair-stack cycle detection.
- **Globals:** VM globals array is now dynamically sized (up to 16M) and reset to `none` before each `apex_run_string`.
- **Async:** `OP_ASYNC_CALL` checks both `malloc`s (future + args); on OOM it halts cleanly instead of dereferencing NULL.
- **Async:** `future_start` now reports OOM, sets `had_error`, and resolves the future with `none` so callers don't wait forever.
- **JIT:** Loops touching globals with index ≥ 64 are correctly guarded, fixing a silent miscompile.
- **JIT:** `rip_fixup_at[]` / `rip_fixup_imm[]` in the x86-64 emitter are now bounds-checked via `RIP_FIXUP_ADD`; overflow raises `JIT_FATAL` instead of corrupting the frame.
- **Scheduler:** `pending_workers` is now `_Atomic int`, removing a C11 data race.
- **Table iteration:** pins the iterated table's register across the loop body; fixes heap-use-after-free when the body reused it.
- **Table iteration:** `break` now pops the table iterator stack via `OP_POP_TABLE_ITER`, not the numeric one.
- **Async:** `vm_execute` only clears `had_error` at top level, so a coroutine's error is no longer wiped by the next coroutine's slice.
- **Runtime:** String intern table init and resize, and `string_create`, now check allocation results.
- **Runtime:** `table_to_string` caps recursion at 128 levels and emits `...` past the cut-off, instead of overflowing the C stack on deeply nested tables.
- **Runtime:** `execute_source` checks `vm_create` for NULL before calling `vm_set_args`.

## Compiler
- **Parser:** Builtin hash table is built under a one-time initializer, removing a race across concurrent hosts.
- **Codegen:** `emit_unswitched_for` frees its transient statement lists (leak fix).
- **Codegen:** `codegen_identifier` skips qualified-name branches when `snprintf` truncates.
- **Codegen:** `assign_registers_linear_scan` grows its arrays on overflow instead of writing past `cap`.
- **Optimizer:** `opt_licm.c` checks `realloc` results before committing; OOM skips a hoist candidate instead of dereferencing NULL.
- **Optimizer:** Functions reassigning a parameter are no longer inlined; callers' variables were silently overwritten.

## Libraries
- **`random`:** All `srand`/`rand` access is serialised under a process-wide mutex.
- **`os.access`:** Mode is interpreted as octal (`755` → `rwxr-xr-x`), matching `chmod`.
- **`json.encode`:** Strings are escaped by length, so embedded NUL bytes round-trip instead of truncating at the first NUL.
- **`os.write` / `os.append`:** writes are length-aware; embedded NULs no longer truncate output. Short writes report `false`.

## Tooling
- **REPL:** Left/right arrow keys navigate the input line.
- **Embedding C API:** Hardened for multi-state hosts — OOM and overflow checks in `stack_grow` / `apex_check_stack`, new `apex_had_error` / `apex_clear_error` accessors, globals reset before each run, builtin hash built once per process. Also updated for embedding Apex in host applications.

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