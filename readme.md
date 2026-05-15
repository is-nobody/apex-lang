# Apex language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Apex Version](https://img.shields.io/badge/Apex-v26.06-blue)](https://apex.org/apex-lang)

**Apex** — a simple and modern programming language with built-in libraries. 
Write once, build native binaries for Windows, macOS and Linux from a single codebase.

Created and maintained by one person. MIT Licensed.

## Quick Start
### Prerequisites
- Apex v26.06

### Install Apex
1. Download the installer from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Run the installer for your OS.
3. Verify installation:

```bash
apex
```

Expected output: `Apex v26.06` or higher.

### Run Your First Program
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

## Building for Distribution
Build native binaries for all enabled platforms:

```bash
apex build main.apex
```

Binaries appear in the `build/` folder.

### Configure Target Platforms
```bash
apex settings
```

This opens an interactive REPL. Type `platforms` to see available targets:

```
Available platforms:
Windows     - Y
macOS       - Y
Linux       - Y
```

Toggle platforms by typing their name. Type `exit` to save and quit.

## Documentation
- **[Apex Reference Manual for Beginners](resources/RM_fBeginners.md)** — variables, data types, operators, control flow, functions, imports, built-in libraries (`os`, `math`, `string`, `network`, `ui`).

## Contributing
We welcome contributions. Before submitting a pull request, please read:

1. **[Code of Conduct](code_of_conduct.md)**
2. **[Contributing Guide](resources/contributing.md)** — dev setup, code style, testing, PR process.

## Support
**[Make a donation](https://example.com)** — funds go to Apex Community Awards for top contributors.

[Learn more about the Apex Community Awards](https://example.com/awards)