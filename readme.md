# Apex language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Apex Version](https://img.shields.io/badge/Apex-v26.06-blue)](https://github.com/is-nobody/apex-lang)
![Avalaible](https://img.shields.io/badge/Avalaible-Windows%20%7C%20macOS%20%7C%20Linux-red)

**Apex** — a simple and modern programming language with built-in libraries. 
Write once, build native binaries for Windows, macOS and Linux from a single codebase.

Created and maintained by one person. MIT Licensed.

## Quick Start
### Install Apex
1. Download the installer from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the installer for your OS.
3. Verify installation:

```bash
apex
```

Expected output: `Apex v26.06`.

### Run Script
Create your own file with `main.apex` name:

```apex
// main.apex
import os
os.output("Hello, Friend")
```

Execute it:

```bash
apex main.apex
```

## Documentation
- **[Apex Reference Manual for Beginners](resources/RM_fBeginners.md)** — variables, data types, operators, control flow, functions, imports, built-in libraries (`os`, `math`, `string`, `network`, `ui`).

## Contributing
We welcome contributions. Please read:

1. **[Code of Conduct](code_of_conduct.md)**
2. **[Contributing Guide](resources/contributing.md)**