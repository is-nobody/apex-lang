<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/logo.png">
    <source media="(prefers-color-scheme: light)" srcset="resources/logo-dark.png">
    <img alt="Apex" src="resources/logo.png" width="75%">
  </picture>
</div>

---

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Apex Version](https://img.shields.io/badge/Apex-26.09-blue)](https://github.com/is-nobody/apex-lang)
![Available](https://img.shields.io/badge/Available-Windows%20%7C%20macOS%20%7C%20Linux-red)

> [!IMPORTANT]
> Apex language is currently in **Open Beta (26.09)** and is being actively developed.

This is the official repository for [Apex](https://github.com/is-nobody/apex-lang) language.

## Why Apex?
- **Simplicity:** Clean, indentation-based syntax with no braces or semicolons. Designed to be readable and easy to learn.

- **Built-in Power:** Comes with a comprehensive standard library (`os`, `sys`, `math`, `string`, `table`, `random`, `json`, `xml`, `csv`, `base`, `regex`, `crypto`, `zip`, `network`) out of the box.

- **Performance:** Fast by design — a register-based VM, NaN-boxing, and an x86-64 JIT compiler, all written in C.

- **Bundle Binaries:** Use `apex build` to compile your source and bundle it with the interpreter into a single standalone executable.

## Quick Start
### Install Apex
Download the Apex language from [GitHub releases](https://github.com/is-nobody/apex-lang/releases), then run `apex version`.

### Build from Source
If you want to build the **bleeding-edge** version directly from the repository:

```bash
git clone https://github.com/is-nobody/apex-lang.git
cd apex-lang

cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/ --parallel
```

### Writing a Tool in Apex
A small utility that reads a file, counts words, and prints a report:

```apex
import os
import regex
import table

function count_words(text)
    words = regex.find_all("\\S+", text)
    return table.size(words)

arg = os.args[1]
content = os.read(arg)

if content == none
    os.output("Cannot read {arg}")
else
    total = count_words(content)
    os.output("Words in {arg}: {total}")
```

Save it as `count.apex`, run it:

```bash
apex count.apex readme.md
```

Or build it into a standalone executable:

```bash
apex build count.apex
./count_x86-64_linux readme.md
```

No project setup, no manifest, no dependency tree. From a single script to a full project — one command, one binary.

## Documentation
Each of the documents explains variables, data types, operators, control flow, functions, imports, and built-in libraries.

- **[Apex Reference Manual for Beginners](resources/RM_fBeginners.md)** — Excellent for beginners in programming, with a detailed explanation of each topic.
- **[Apex Express Course for Developers](resources/EC_fDevelopers.md)** — Excellent for developers, offering minimal and clear coverage of each topic.
- **[Library Reference](resources/Library_Reference.md)** — The standard library, module by module.

## Getting Help
See [Issues](https://github.com/is-nobody/apex-lang/issues) for bug reports and feature requests.

## Contributing
Apex is created and maintained by one person, but contributions are welcome!

Please see [contributing](contributing.md) and remember about [Code of Conduct](code_of_conduct.md)

## License
Apex is distributed under the terms of the **MIT license**.

See [license](license) for details.