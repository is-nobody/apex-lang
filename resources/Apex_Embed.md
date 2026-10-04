# Apex Embedding in C/C++ (26.10)
This manual is minimalistic. Each section builds on the previous ones. For the best experience, follow the order.

## Table of Contents
### Introduction
- [What Embedding Means](#what-embedding-means)
- [What You Get](#what-you-get)
- [What You Don't Get](#what-you-dont-get)

### Setup
- [Files You Need](#files-you-need)
- [Compiling the Host](#compiling-the-host)
- [A Minimal Host Program](#a-minimal-host-program)

### State
- [Opening and Closing](#opening-and-closing)
- [The State Handle](#the-state-handle)

### The Stack
- [Why a Stack](#why-a-stack)
- [Pushing Values](#pushing-values)
- [Reading Values](#reading-values)
- [Manipulating the Stack](#manipulating-the-stack)
- [Stack Indices](#stack-indices)

### Running Apex
- [From a String](#from-a-string)
- [From a File](#from-a-file)
- [What Happens Under the Hood](#what-happens-under-the-hood)

### Calling Apex from C
- [Reading Globals](#reading-globals)
- [Writing Globals](#writing-globals)
- [Reading Tables](#reading-tables)
- [Writing Tables](#writing-tables)

### Calling C from Apex
- [Registering a Function](#registering-a-function)
- [Reading Arguments](#reading-arguments)
- [Returning Values](#returning-values)
- [Raising Errors](#raising-errors)

### Types
- [The Type Enum](#the-type-enum)
- [Type Checks](#type-checks)
- [Conversions](#conversions)
- [Checked Accessors](#checked-accessors)

### Values
- [Numbers](#numbers)
- [Strings](#strings)
- [Booleans](#booleans)
- [None](#none)
- [Tables](#tables)
- [Light Userdata](#light-userdata)
- [Futures](#futures)

### Errors
- [The Error Model](#the-error-model)
- [Reading the Last Error](#reading-the-last-error)
- [Raising from C](#raising-from-c)
- [Exit Requests](#exit-requests)

### Complete Examples
- [A Calculator Host](#a-calculator-host)
- [Exposing a C Struct](#exposing-a-c-struct)
- [A Config Reader](#a-config-reader)
- [Honouring `os.exit`](#honouring-osexit)

### Rules and Restrictions
- [1. No Closures](#1-no-closures)
- [2. No Full Userdata](#2-no-full-userdata)
- [3. No Metatables](#3-no-metatables)
- [4. No Direct Apex-to-Apex Calls from C](#4-no-direct-apex-to-apex-calls-from-c)
- [5. Explicit Booleans Only](#5-explicit-booleans-only)
- [6. One Reference Per Push](#6-one-reference-per-push)
- [7. C Callback Registration Is Per-State](#7-c-callback-registration-is-per-state)
- [8. C++ Requires extern "C"](#8-c-requires-extern-c)

# Introduction
## What Embedding Means
Embedding Apex means taking the Apex interpreter and putting it **inside** your C or C++ program. Your program becomes the host. Apex becomes a scripting layer that runs inside it.

Three things you can do after embedding:

1. **Run Apex scripts** from your C program (from a string, a file, or a buffer).
2. **Read Apex values** — look up globals, walk tables, read strings and numbers.
3. **Expose C functions** to Apex, so scripts can call back into your program.

## What You Get
The embedding API is a single pair of files:

| File | What It Contains |
|------|------------------|
| `source/utils/apex_api.h` | Public API — the only header you include |
| `source/utils/apex_api.c` | Implementation, compiled into your binary |

Everything else (the compiler, the VM, the standard library) is linked in automatically.

## What You Don't Get
Apex is a small language by design. Some things a scripting host might expect are intentionally missing:

| Feature | Status | Why |
|---------|--------|-----|
| Closures / upvalues | Not supported | Apex functions do not capture state |
| Full userdata with a GC finalizer | Not supported | Apex has no GC and no `__gc` hook |
| Metatables / metamethods | Not supported | Apex values have no per-value behavior hooks |
| Calling an Apex function value from C | Not in this API | No `apex_call`; see the workaround below |
| Coroutines (`resume` / `yield`) | Not in this API | Futures exist but aren't first-class from C |

If your design depends on any of these, you'll need a different approach. See [Rules and Restrictions](#rules-and-restrictions) for the full list.

# Setup
## Files You Need
Two files are added to your project, both already shipped with Apex:

```
your-project/
├── apex_api.h      <- copy from apex-lang/source/utils/
└── apex_api.c      <- copy from apex-lang/source/utils/
```

Alongside them, you also need the entire Apex source tree, because `apex_api.c` includes the compiler, VM, and standard library. The easiest approach is to build Apex as a **static library** and link it into your application.

## Compiling the Host
Create a static library from the Apex sources:

```bash
cmake -S /path/to/apex-lang -B /path/to/apex-lang/build -DCMAKE_BUILD_TYPE=Release
cmake --build /path/to/apex-lang/build --parallel
```

Then link your host against the resulting `libapex.a` (or `apex.lib` on Windows), plus the platform libraries Apex already depends on:

| Platform | Extra Libraries |
|----------|-----------------|
| Linux | `-lpthread -lm -lz -ldl` |
| macOS | `-lpthread -lm -lz` |
| Windows | `ws2_32 shlwapi` |

A minimal `CMakeLists.txt` for your host:

```cmake
cmake_minimum_required(VERSION 3.13)
project(MyHost C)

add_executable(my_host main.c /path/to/apex-lang/source/utils/apex_api.c)

target_include_directories(my_host PRIVATE
    /path/to/apex-lang/source/utils
    /path/to/apex-lang/source/core
    /path/to/apex-lang/source/compiler
    /path/to/apex-lang/source/libraries
)

# Link the whole apex tree as sources, or link libapex.a if you built it
# (see the apex-lang CMakeLists.txt for the full list of .c files)
```

## A Minimal Host Program
Before diving into the API, here is the shortest complete host. It opens a state, runs a script, and closes the state:

```c
#include "apex_api.h"

int main(void) {
    ApexState* S = apex_open();
    if (!S) return 1;

    int rc = apex_run_string(S,
        "import os\n"
        "os.output(\"Hello from embedded Apex!\")\n",
        "hello.apex");

    if (rc != 0) {
        fprintf(stderr, "error: %s\n", apex_last_error(S));
    }

    apex_close(S);
    return rc;
}
```

Build it, run it, and you should see `Hello from embedded Apex!` printed by the Apex interpreter.

# State
## Opening and Closing
An `ApexState` is the entire universe of one embedded Apex interpreter. Everything happens inside it: the VM, the globals, the string intern table, the value stack.

Create one with `apex_open()`:

```c
ApexState* S = apex_open();
```

Destroy it with `apex_close()`:

```c
apex_close(S);
```

Every value ever produced by the state — including strings, tables, and futures — is released when the state is closed. You do not need to free them yourself.

**One state per thread.** Apex's scheduler uses per-state worker threads and per-state completion queues. Running two threads against the same state is not supported.

## The State Handle
`ApexState` is opaque. You never see its fields. Everything is done through function calls that take `S` as their first argument.

For advanced use, `apex_get_vm(S)` returns a `struct VM*`. You should not need this unless you are reaching into Apex internals.

# The Stack
## Why a Stack
All communication between C and Apex happens through a **stack of values**. This design keeps the API small and uniform:

- Every function takes the same simple set of arguments.
- No unions, no `void*` gymnastics, no per-type entry points.
- Any Apex value of any type can be passed through it without special-casing.

Think of the stack as a scratchpad. You push values onto it, Apex reads from it, Apex pushes values back, and you read them off. Every operation happens at the top of the stack or at a specific index into it.

## Pushing Values
Push a value of any type:

```c
apex_push_none(S);              // none
apex_push_boolean(S, 1);        // true
apex_push_integer(S, 42);       // whole number
apex_push_number(S, 3.14);      // decimal number
apex_push_string(S, "hello");   // string
apex_push_light_userdata(S, p); // opaque C pointer
apex_new_table(S);              // empty table
```

Each of these puts one value on top of the stack and increments `apex_get_top(S)` by one.

## Reading Values
Read the value at any index:

```c
int         b = apex_to_boolean(S, -1);   // boolean as int (0 or 1)
long long   n = apex_to_integer(S, -1);   // number as long long
double      d = apex_to_number(S, -1);    // number as double
const char* s = apex_to_string(S, -1);    // string as const char*
void*       p = apex_to_light_userdata(S, -1);  // userdata as void*
```

`apex_to_string` returns a pointer to the string's internal buffer. You must not free it. The pointer is valid as long as the string lives on the stack or in an Apex table.

If the value at that index has the wrong type, `apex_to_*` returns a safe default (0, `NULL`, `"none"`). To check the type first, see [Types](#types).

## Manipulating the Stack
Basic stack management:

| Function | What It Does |
|----------|--------------|
| `apex_get_top(S)` | Return the number of elements on the stack |
| `apex_set_top(S, n)` | Truncate or pad the stack to size `n` |
| `apex_pop(S, n)` | Pop `n` elements off the top |
| `apex_check_stack(S, n)` | Ensure room for `n` more elements |

Copy and rearrange:

| Function | What It Does |
|----------|--------------|
| `apex_push_value(S, i)` | Push a copy of the value at index `i` |
| `apex_remove(S, i)` | Remove the value at index `i`, shifting down |
| `apex_insert(S, i)` | Move the top value to index `i`, shifting up |
| `apex_replace(S, i)` | Pop the top value and put it at index `i` |

## Stack Indices
Every index can be **positive** or **negative**.

| Form | Meaning |
|------|---------|
| Positive (`1, 2, 3...`) | Absolute position from the bottom. Index `1` is always the first value ever pushed. |
| Negative (`-1, -2, -3...`) | Position relative to the top. Index `-1` is always the top. |

Negative indices are the ones you will use most often, because they don't depend on how many values are already on the stack.

```c
// Top of stack is at index -1, below it is -2, and so on
const char* top_string = apex_to_string(S, -1);
```

When a function like `apex_get_field` takes an index, that index can be positive or negative with the same meaning.

# Running Apex
## From a String
`apex_run_string` compiles and runs Apex source:

```c
int rc = apex_run_string(S,
    "import os\n"
    "os.output(\"Hello, World!\")\n",
    "hello.apex");   // chunk name, used in errors and imports

if (rc != 0) {
    fprintf(stderr, "%s\n", apex_last_error(S));
} else if (apex_exit_requested(S)) {
    return apex_exit_code(S);          // script called os.exit(n)
}
```

The chunk name is what appears in error messages and what the import resolver uses to look up files. Use a real path if the script imports files. Use `"<string>"` if it doesn't.

If the script calls `os.exit(n)`, `apex_run_string` still returns `0` — an exit is not an error. To distinguish a normal completion from a script-requested exit, call `apex_exit_requested(S)` after a successful return, and read the code with `apex_exit_code(S)`. See [Exit Requests](#exit-requests).

## From a File
`apex_run_file` reads the whole file into memory and runs it:

```c
int rc = apex_run_file(S, "scripts/startup.apex");
if (rc == 0 && apex_exit_requested(S)) {
    return apex_exit_code(S);
}
```

Imports inside that file will be resolved relative to the file's directory, exactly as if the file had been run from the command line.

## What Happens Under the Hood
When you call `apex_run_string`, the API does the whole pipeline for you:

1. Tokenize the source.
2. Parse it into an AST.
3. Run all compiler optimizations.
4. Generate bytecode.
5. Run the bytecode in the VM.

The **most recent chunk stays alive** after the call returns. Globals and any tables reachable from them remain accessible to your C code. This is what makes `apex_get_global` work — see the next section.

Running a new string destroys the old chunk. Its globals and code are gone. If you need state to survive across runs, store it in a **C-owned table** on the C side, not in Apex globals.

# Calling Apex from C
## Reading Globals
After running a script, read a global into the stack with `apex_get_global`:

```c
apex_run_string(S,
    "config = [\"host\" = \"localhost\", \"port\" = 8080]\n",
    "config.apex");

if (apex_get_global(S, "config")) {
    // The table is now on top of the stack
    if (apex_is_table(S, -1)) {
        apex_get_field(S, -1, "host");         // pushes config["host"]
        const char* host = apex_to_string(S, -1);
        printf("host = %s\n", host);
        apex_pop(S, 1);                        // drop the string
    }
    apex_pop(S, 1);                            // drop the table
}
```

`apex_get_global` returns `1` if the global exists and `0` otherwise. On success, the value is pushed onto the stack.

## Writing Globals
Push a value, then hand it to `apex_set_global`:

```c
apex_push_string(S, "127.0.0.1");
apex_set_global(S, "server_ip");
```

`apex_set_global` **pops** the value. If the global doesn't exist in the current chunk, the value is silently dropped.

## Reading Tables
Reading a field pushes the result onto the stack:

```c
apex_get_global(S, "config");                  // config table on stack
apex_get_field(S, -1, "port");                 // config["port"] on stack
long long port = apex_to_integer(S, -1);
apex_pop(S, 2);                                // drop value and table
```

Numeric indices work the same way:

```c
apex_get_index(S, -1, 1);                      // config[1], whatever it is
```

Reading a missing key pushes `none`. There is no error.

## Writing Tables
Pushing a value and calling a `set` function writes into a table:

```c
apex_get_global(S, "config");                  // config table on stack at -1
apex_push_string(S, "example.com");            // value to set
apex_set_field(S, -2, "host");                 // config["host"] = "example.com"
apex_pop(S, 1);                                // drop the table
```

Notice the `-2` — the table is one below the value we just pushed. `apex_set_field` **pops the value** but leaves the table in place.

# Calling C from Apex
## Registering a Function
Register a C function under a name. Apex code will then see it as a callable function:

```c
static int host_add(ApexState* S) {
    double a = apex_check_number(S, 1);
    double b = apex_check_number(S, 2);
    apex_push_number(S, a + b);
    return 1;                          // we pushed one result
}

int main(void) {
    ApexState* S = apex_open();
    apex_register_function(S, "host_add", host_add);

    apex_run_string(S, "import os\n"
                       "os.output(host_add(2, 3))\n",   // prints 5
                    "test.apex");
    apex_close(S);
    return 0;
}
```

The registration is **per-state**. Opening a second state means calling `apex_register_function` again.

## Reading Arguments
Arguments arrive already pushed onto the state's stack, in order:

| Argument | Index |
|----------|-------|
| First argument | `1` |
| Second argument | `2` |
| ... | ... |
| Last argument | `apex_get_top(S)` |

Because you don't know the count in advance, the safest approach is to use the **checked** accessors:

```c
static int host_greet(ApexState* S) {
    const char* name = apex_check_string(S, 1);   // raise if not a string
    apex_push_string(S, name);
    return 1;
}
```

`apex_check_string` raises an error if argument 1 isn't a string. See [Errors](#errors) for how raising works.

## Returning Values
Push your return value onto the stack, then return the **number of results**:

```c
static int host_double(ApexState* S) {
    double x = apex_check_number(S, 1);
    apex_push_number(S, x * 2);
    return 1;                          // one result
}

static int host_noop(ApexState* S) {
    return 0;                          // no results; Apex sees none
}
```

Apex functions always return exactly one value. If you return `0`, Apex receives `none`.

## Raising Errors
`apex_raise` formats a message and, if a protected context is active, longjmps back to it:

```c
static int host_div(ApexState* S) {
    double a = apex_check_number(S, 1);
    double b = apex_check_number(S, 2);
    if (b == 0.0) {
        apex_raise(S, "division by zero");
        return 0;                      // not reached if raise longjmps
    }
    apex_push_number(S, a / b);
    return 1;
}
```

After `apex_raise`, the VM marks the whole run as failed. The host learns about it through the return value of `apex_run_string`.

# Types
## The Type Enum
`apex_type` returns one of these:

| Value | Meaning |
|-------|---------|
| `APEX_TNONE` | none |
| `APEX_TBOOLEAN` | true / false |
| `APEX_TNUMBER` | number (integer or decimal) |
| `APEX_TSTRING` | string |
| `APEX_TTABLE` | table |
| `APEX_TFUNCTION` | Apex function reference |
| `APEX_TLIGHTUSERDATA` | opaque C pointer |
| `APEX_TFUTURE` | pending or resolved future |

## Type Checks
Predicates return `1` if the value at the index has the given type, `0` otherwise:

```c
if (apex_is_string(S, -1)) {
    const char* s = apex_to_string(S, -1);
    // safe to use s
}
```

Available: `apex_is_none`, `apex_is_boolean`, `apex_is_number`, `apex_is_string`, `apex_is_table`, `apex_is_function`, `apex_is_light_userdata`, `apex_is_future`.

`apex_type_name(S, index)` returns the type name as a C string, useful for logging.

## Conversions
Non-checked conversions return a safe default on type mismatch:

| Function | Returns on Mismatch |
|----------|---------------------|
| `apex_to_boolean` | `0` |
| `apex_to_integer` | `0` |
| `apex_to_number` | `0.0` |
| `apex_to_string` | `NULL` |
| `apex_to_light_userdata` | `NULL` |
| `apex_to_pointer` | `NULL` |

## Checked Accessors
Checked accessors raise an error on type mismatch, then return the value. Use these when a wrong type is a bug rather than a normal case:

| Function | Checks | Returns |
|----------|--------|---------|
| `apex_check_integer(S, n)` | number | `long long` |
| `apex_check_number(S, n)` | number | `double` |
| `apex_check_string(S, n)` | string | `const char*` |
| `apex_check_lstring(S, n, len)` | string | `const char*` + length |
| `apex_check_light_userdata(S, n)` | userdata | `void*` |
| `apex_check_type(S, n, type)` | any specific type | (void) |

The `n` argument is a **1-based argument number**, not a stack index. `apex_check_string(S, 1)` checks the first argument.

# Values
## Numbers
Apex has a single numeric type. Whole numbers and decimals are both `double` internally.

```c
apex_push_integer(S, 42);            // push 42
apex_push_number(S, 3.14);           // push 3.14
double d = apex_to_number(S, -1);    // read back as double
long long i = apex_to_integer(S, -1); // read back truncated to integer
```

## Strings
Strings are immutable and reference-counted. `apex_push_string` copies the bytes; you can free your copy immediately after.

```c
char buf[256];
snprintf(buf, sizeof(buf), "user_%d", id);
apex_push_string(S, buf);            // safe, string is copied
```

`apex_to_string` returns a pointer **into the string object's own memory**. Do not free it. The pointer is valid as long as the string is reachable.

For strings that may contain embedded zero bytes, use `apex_push_lstring` and `apex_to_lstring`:

```c
apex_push_lstring(S, data, data_len);           // length-aware push
size_t len;
const char* s = apex_to_lstring(S, -1, &len);   // length-aware read
```

## Booleans
Apex booleans are `true` and `false`, and nothing else. There is no truthy/falsy conversion.

```c
apex_push_boolean(S, 1);             // true
apex_push_boolean(S, 0);             // false
apex_push_boolean(S, 42);            // still true: any non-zero becomes true

int b = apex_to_boolean(S, -1);      // 1 for true, 0 for everything else
```

`apex_to_boolean` returns `1` only if the value is literally `true`. It returns `0` for `false` and for every non-boolean value.

## None
`none` is the absence of a value. It shows up whenever a table lookup misses, a function returns nothing, or you intentionally push it:

```c
apex_push_none(S);
if (apex_is_none(S, -1)) { /* ... */ }
```

## Tables
Tables hold mixed positional values and key-value pairs. Both numeric and string keys are allowed.

```c
apex_new_table(S);                         // table on stack at -1

apex_push_string(S, "Alice");
apex_set_field(S, -2, "name");             // tbl["name"] = "Alice"

apex_push_integer(S, 42);
apex_set_index(S, -2, 1);                  // tbl[1] = 42

long long n = apex_table_size(S, -1);      // 2
```

Field access:

```c
apex_get_field(S, -1, "name");             // pushes tbl["name"]
apex_get_index(S, -1, 1);                  // pushes tbl[1]
```

## Light Userdata
Light userdata wraps a raw C pointer as an Apex value. It's opaque on the Apex side — scripts can store it and pass it back, but not inspect it.

```c
struct Context { int fd; /* ... */ };
struct Context ctx = { .fd = 3 };

apex_push_light_userdata(S, &ctx);         // wrap the pointer
apex_set_global(S, "ctx");

// ... later, inside a C function called by Apex:
void* p = apex_check_light_userdata(S, 1);
struct Context* c = (struct Context*)p;
```

Light userdata is **pointer-size limited**: on x86-64 and arm64, the low 48 bits of the pointer are stored. This is enough for any user-space pointer but not for kernel-space addresses or >48-bit architectures.

## Futures
Apex's async/await system produces **futures**. From C, you can detect them but not drive them directly:

```c
if (apex_is_future(S, -1)) {
    // handle a future value
}
```

`apex_run_string` and `apex_run_file` drive the scheduler to completion. When the top-level script returns, all pending futures have either been awaited or discarded.

# Errors
## The Error Model
Apex has **errors as values** — there is no `try`/`catch`, no exceptions, and no return codes for Apex code itself. But when you embed Apex in C, you still need to know if the run failed.

The embedding API reports errors through three mechanisms:

1. `apex_run_string` and `apex_run_file` return non-zero on failure.
2. `apex_last_error(S)` returns a human-readable message.
3. C functions can raise an error with `apex_raise`, which longjmps back to a protected context set up by the API's dispatcher.

An **exit request** is a distinct outcome: a script that calls `os.exit(n)` terminates cleanly, so the run returns `0`, and the host must query `apex_exit_requested` / `apex_exit_code` to honour it. See [Exit Requests](#exit-requests).

## Reading the Last Error
After a failed run:

```c
int rc = apex_run_string(S, "invalid apex code !!!", "bad.apex");
if (rc != 0) {
    fprintf(stderr, "apex error: %s\n", apex_last_error(S));
}
```

The message buffer is owned by the state and is overwritten by the next error. Copy it if you need to keep it.

## Raising from C
Inside a registered C function, call `apex_raise` to signal a failure:

```c
static int host_open(ApexState* S) {
    const char* path = apex_check_string(S, 1);
    FILE* f = fopen(path, "rb");
    if (!f) {
        apex_raise(S, "cannot open file '%s'", path);
        return 0;   // not reached when the API's protected context is active
    }
    apex_push_light_userdata(S, f);
    return 1;
}
```

`apex_raise` marks the error on the state and longjmps back to the API's dispatcher, which propagates the failure to `vm->had_error`. The next `apex_run_string` call will see it as a failure.

## Exit Requests
A script that calls `os.exit(n)` stops the VM the same way a normal top-level `return` does. The run is **not** a failure — `apex_run_string` and `apex_run_file` still return `0`, and `apex_had_error(S)` still returns `0`. Instead, the VM records the request, and the host must inspect it explicitly:

```c
int rc = apex_run_string(S, code, "script.apex");
if (rc != 0) {
    fprintf(stderr, "error: %s\n", apex_last_error(S));
    return 1;
}
if (apex_exit_requested(S)) {
    return apex_exit_code(S);          // honour os.exit(n)
}
return 0;                              // normal completion
```

Three accessors cover the lifecycle:

| Function                 | What It Does                                                          |
|--------------------------|-----------------------------------------------------------------------|
| `apex_exit_requested(S)` | Returns `1` if the most recent run called `os.exit`, `0` otherwise    |
| `apex_exit_code(S)`      | Returns the code passed to `os.exit`, or `0` if no exit was requested |
| `apex_clear_exit(S)`     | Clears the pending exit request and its code                          |

`apex_run_string` clears any stale exit state at the start of every run, so a script that exits followed by a script that does not will report `apex_exit_requested(S) == 0` after the second run. You only need `apex_clear_exit` if you want to reset the flag *without* running another script — for example, after logging the exit code and before handing the state to a different code path.

An exit request does not suppress errors: if the script both raises an error and calls `os.exit`, the error wins. Check `rc` first, then `apex_exit_requested`.

# Complete Examples
## A Calculator Host
A host program that exposes a `host_add` function to Apex:

```c
#include "apex_api.h"
#include <stdio.h>

static int host_add(ApexState* S) {
    double a = apex_check_number(S, 1);
    double b = apex_check_number(S, 2);
    apex_push_number(S, a + b);
    return 1;
}

static int host_mul(ApexState* S) {
    double a = apex_check_number(S, 1);
    double b = apex_check_number(S, 2);
    apex_push_number(S, a * b);
    return 1;
}

int main(void) {
    ApexState* S = apex_open();
    if (!S) return 1;

    apex_register_function(S, "host_add", host_add);
    apex_register_function(S, "host_mul", host_mul);

    int rc = apex_run_string(S,
        "import os\n"
        "a = host_add(2, 3)\n"
        "b = host_mul(a, 4)\n"
        "os.output(\"result: {b}\")\n",
        "calc.apex");

    if (rc != 0) {
        fprintf(stderr, "error: %s\n", apex_last_error(S));
    }

    apex_close(S);
    return rc;
}
```

## Exposing a C Struct
Pass an opaque C struct to Apex scripts and back:

```c
#include "apex_api.h"
#include <stdlib.h>
#include <math.h>

typedef struct { int x, y; } Point;

static int host_new_point(ApexState* S) {
    int x = (int)apex_check_integer(S, 1);
    int y = (int)apex_check_integer(S, 2);
    Point* p = malloc(sizeof(Point));
    p->x = x;
    p->y = y;
    apex_push_light_userdata(S, p);
    return 1;
}

static int host_distance(ApexState* S) {
    Point* a = (Point*)apex_check_light_userdata(S, 1);
    Point* b = (Point*)apex_check_light_userdata(S, 2);
    int dx = a->x - b->x;
    int dy = a->y - b->y;
    double d = sqrt((double)(dx*dx + dy*dy));
    apex_push_number(S, d);
    return 1;
}

static int host_free_point(ApexState* S) {
    Point* p = (Point*)apex_check_light_userdata(S, 1);
    free(p);
    return 0;
}

int main(void) {
    ApexState* S = apex_open();
    apex_register_function(S, "new_point",  host_new_point);
    apex_register_function(S, "distance",   host_distance);
    apex_register_function(S, "free_point", host_free_point);

    apex_run_string(S,
        "import os\n"
        "a = new_point(0, 0)\n"
        "b = new_point(3, 4)\n"
        "os.output(\"distance: {distance(a, b)}\")\n"
        "free_point(a)\n"
        "free_point(b)\n",
        "points.apex");

    apex_close(S);
    return 0;
}
```

Notice that the *host* manages the lifetime of `Point`. Light userdata holds the pointer, but the C side decides when to free it. Use `apex_register_function` to expose a `free_point` function to scripts, or track pointers yourself.

## A Config Reader
A host that runs a config script and reads the resulting table:

```c
#include "apex_api.h"
#include <stdio.h>

int main(void) {
    ApexState* S = apex_open();

    apex_run_string(S,
        "config = [\n"
        "    \"host\" = \"localhost\",\n"
        "    \"port\" = 8080,\n"
        "    \"debug\" = true,\n"
        "    \"tags\" = [\"web\", \"prod\"]\n"
        "]\n",
        "config.apex");

    if (!apex_get_global(S, "config")) {
        fprintf(stderr, "config is not defined\n");
        apex_close(S);
        return 1;
    }

    apex_get_field(S, -1, "host");
    printf("host: %s\n", apex_to_string(S, -1));
    apex_pop(S, 1);

    apex_get_field(S, -1, "port");
    printf("port: %lld\n", apex_to_integer(S, -1));
    apex_pop(S, 1);

    apex_get_field(S, -1, "debug");
    printf("debug: %s\n", apex_to_boolean(S, -1) ? "yes" : "no");
    apex_pop(S, 1);

    apex_get_field(S, -1, "tags");
    long long n = apex_table_size(S, -1);
    for (long long i = 1; i <= n; i++) {
        apex_get_index(S, -1, i);
        printf("tag: %s\n", apex_to_string(S, -1));
        apex_pop(S, 1);
    }
    apex_pop(S, 2);   // tags table and config table

    apex_close(S);
    return 0;
}
```

## Honouring `os.exit`
A host that runs a user-supplied script and returns the script's exit code to the operating system:

```c
#include "apex_api.h"
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <script.apex>\n", argv[0]);
        return 2;
    }

    ApexState* S = apex_open();
    if (!S) return 2;

    int rc = apex_run_file(S, argv[1]);
    if (rc != 0) {
        fprintf(stderr, "apex: %s\n", apex_last_error(S));
        apex_close(S);
        return 1;
    }

    int code = apex_exit_requested(S) ? apex_exit_code(S) : 0;
    apex_close(S);
    return code;
}
```

If the script does not call `os.exit`, the host returns `0`. If it calls `os.exit(42)`, the host returns `42`. A compile or runtime error returns `1`, distinct from both.

# Rules and Restrictions
## 1. No Closures
Apex has no closures, no upvalues, and no captured variables. A function value cannot carry hidden state. If your design needs a callback with state, pass the state as an argument or use a table.

## 2. No Full Userdata
There is no full userdata. You cannot allocate a heap block that the VM will track and finalize. Use **light userdata** for pointers, and manage lifetime yourself.

## 3. No Metatables
Apex values have no metatables and no metamethods. `__index`, `__newindex`, `__add`, `__gc`, and every other metamethod do not exist. If you need custom behavior, wrap your data in a table and provide C functions that operate on it.

## 4. No Direct Apex-to-Apex Calls from C
The API does not provide `apex_call`. You cannot take a function value from Apex and call it directly with C-supplied arguments.

If you need this, the workaround is:

1. Run a small Apex chunk that performs the call.
2. Store the result in a global.
3. Read the global from C.

```c
apex_push_string(S, "some_arg");
apex_set_global(S, "__arg");

apex_run_string(S,
    "__result = my_function(__arg)\n",
    "<call bridge>");

apex_get_global(S, "__result");
```

This is slower but correct.

## 5. Explicit Booleans Only
Apex has no truthy/falsy. Only `true` is true. `apex_to_boolean` returns `1` for `true` and `0` for everything else, including non-zero numbers, non-empty strings, and non-empty tables.

## 6. One Reference Per Push
Every push hands a new reference to the state's stack. Every pop that discards a value releases that reference. You do not need to call `value_incref` or `value_decref` yourself — the API does this for you.

The only exception is `apex_push_string` and `apex_push_lstring`, which copy the string's bytes. The state keeps its own reference. You can free your copy immediately.

## 7. C Callback Registration Is Per-State
`apex_register_function` operates on one state at a time. If your host has multiple states, register the callback on each of them.

## 8. C++ Requires `extern "C"`
The API header already wraps itself in `extern "C"` guards. You can include it directly from C++ without additional wrapping:

```cpp
#include "apex_api.h"     // works in C++ as-is
```

Do not compile `apex_api.c` as C++. It is written as C and must be compiled with a C compiler. In CMake:

```cmake
set_source_files_properties(apex_api.c PROPERTIES LANGUAGE C)
```

# Conclusion
You now know the complete embedding API.

- **Open and close** a state with `apex_open` and `apex_close`.
- **Run Apex code** with `apex_run_string` or `apex_run_file`.
- **Move values** through the stack with `apex_push_*` and `apex_to_*`.
- **Read and write globals and tables** with `apex_get_global`, `apex_get_field`, `apex_set_global`, `apex_set_field`, and their variants.
- **Expose C functions** with `apex_register_function`.
- **Raise errors** with `apex_raise`.

Everything else — the compiler, the VM, the standard library, the scheduler — is hidden behind those calls. The API surface is deliberately small: it exposes exactly what a host needs and nothing more. If a concept isn't in Apex itself (closures, metatables, full userdata), it isn't in the API either.