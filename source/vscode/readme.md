# Apex for Visual Studio Code
A Visual Studio Code extension providing syntax highlighting, code snippets, IntelliSense, and execution support for the **Apex** programming language.

![Version](https://img.shields.io/badge/Version-26.06-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## About Apex
Apex is a simple, cross-platform programming language designed for beginners and professional developers alike. It features a minimalistic indentation-based syntax, extensive built-in libraries for system interaction, file management, mathematics, data encoding, and more. It is distributed under the MIT License.

This extension brings full IDE support to VS Code, making it easier to write, read, and run Apex code.

## Features
### Syntax Highlighting
Full syntax highlighting for:
- **Keywords**: `function`, `if`, `elif`, `else`, `for`, `import`, `and`, `or`, `not`, `break`, `continue`, `return`.
- **Literals**: Numbers (including scientific notation), strings, booleans (`true`, `false`).
- **Comments**: Line comments (`//`).
- **Operators**: Arithmetic (`+`, `-`, `*`, `/`, `%`), Comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), Logical.
- **Structures**: Function definitions, table literals `[]`, string interpolation `"{}"`.

### Advanced IntelliSense & Autocompletion
Smart suggestions for:
- **Keywords**: Control flow and logical operators.
- **8 Standard Libraries**: `os`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `codecs`.
- **Library Functions**: Auto-complete for hundreds of functions like `os.read()`, `sys.platform`, `random.randint()`, `codecs.json_write()`, etc.

### Run Code
Execute your current Apex file directly from VS Code.
- **Command Palette**: `Apex: Run Current File`
- **Keyboard Shortcut**: `F5` (when editing an `.apex` file)
- **Terminal**: Runs the file in a dedicated "Apex" terminal instance.

### Language Configuration
- **Auto-closing**: Supports automatic closing of parentheses `()`, brackets `[]`, and quotes `""`.
- **Indentation Rules**: Smart indentation for blocks following `function`, `if`, `elif`, `else`, and `for`.

## Installation
1. Install the [Apex Interpreter](https://github.com/is-nobody/apex-lang/releases) on your system.
   - Ensure the `apex` command is available in your system's PATH.
2. Install this extension from the VS Code Marketplace or by installing the `.vsix` file.
3. Open any file with the `.apex` extension.

## Standard Library Overview
The Apex runtime includes a powerful set of built-in modules:

| Module | Description |
| ------ | ----------- |
| **os** | System interaction, File I/O, and Process management |
| **sys** | System information |
| **math** | Mathematical functions |
| **string** | String manipulation |
| **table** | Data structures |
| **ffi** | Foreign Function Interface |
| **random** | Random generation |
| **codecs** | Data encoding |

## Release Notes
### 26.06 (June 30, 2026)
- Initial release.
- Syntax highlighting for Apex language.
- IntelliSense for keywords and all 8 built-in libraries.
- Hover documentation for keywords and libraries.
- Command to run current file.
- Document symbol provider for functions.
- Support for scientific notation and bracket auto-closing.