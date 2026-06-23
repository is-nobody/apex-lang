<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/logo.png">
    <source media="(prefers-color-scheme: light)" srcset="resources/logo-dark.png">
    <img alt="Apex" src="resources/logo.png" width="75%">
  </picture>
</div>

---

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Apex Version](https://img.shields.io/badge/Apex-26.6-blue)](https://github.com/is-nobody/apex-lang)
![Available](https://img.shields.io/badge/Available-Windows%20%7C%20macOS%20%7C%20Linux-red)

This is the official repository for [Apex](https://github.com/is-nobody/apex-lang) language.

## Why Apex?
- **Simplicity:** Clean, indentation-based syntax with no braces or semicolons. Designed to be readable and easy to learn.

- **Built-in Power:** Comes with a comprehensive standard library (`os`, `files`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `codecs`) out of the box. No need to hunt for packages for common tasks.

- **Performance:** Written in C with a register-based virtual machine and optimized bytecode execution. Supports cross-compilation and static linking for portable binaries.

## Quick Start
### Install Apex
1. Download the interpreter from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the interpreter for your OS.

**Now you're in REPL!**

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

### Build from Source
If you want to build the **bleeding-edge** version directly from the repository:

```bash
git clone https://github.com/is-nobody/apex-lang.git
cd apex-lang

cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --parallel
```

## Documentation
Each of the documents explains variables, data types, operators, control flow, functions, imports, and built-in libraries.

- **[Apex Reference Manual for Beginners](resources/RM_fBeginners.md)** — Excellent for beginners in programming, with a detailed explanation of each topic.
- **[Apex Reference Manual for Developers](resources/RM_fDevelopers.md)** — Excellent for developers, offering minimal and clear coverage of each topic.

## Getting Help
See [Issues](https://github.com/is-nobody/apex-lang/issues) for bug reports and feature requests.

## Contributing
Apex is created and maintained by one person, but contributions are welcome!

For a detailed explanation of the interpreter's architecture, see the source code in `source/core/`.

## License
Apex is distributed under the terms of the **MIT license**.

See [license](license) for details.