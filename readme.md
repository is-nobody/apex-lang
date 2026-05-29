# Apex language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Apex Version](https://img.shields.io/badge/Apex-v26.06-blue)](https://github.com/is-nobody/apex-lang)
![Available](https://img.shields.io/badge/Available-Windows%20%7C%20macOS%20%7C%20Linux-red)

**Apex** — a simple and modern programming language with built-in libraries. 

Created and maintained by one person. MIT Licensed.

## Getting Started
### Install Apex
1. Download the interpreter from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the interpreter for your OS.

Now you're in REPL!

### Testing the interpreter
Paste this code into REPL:

```apex
import os
os.output("Hello, Friend")
```

Output:

```bash
Hello, Friend
```

### Build binaries
Run in the terminal:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

This will create a binary file for your current platform.

## Documentation
Each of the documentations explains variables, data types, operators, control flow, functions, imports, built-in libraries (`os`, `math`, `string`, `table`).
- **[Apex Reference Manual for Beginners](resources/RM_fBeginners.md)**  — excellent for beginners in programming, a detailed explanation of each topic.
- **[Apex Reference Manual for Developers](resources/RM_fDevelopers.md)** — excellent for developers, minimal and clear coverage of each topic.