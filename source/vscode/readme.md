# Apex Language Support
A Visual Studio Code extension providing syntax highlighting, code snippets, IntelliSense, and execution support for the **Apex** programming language.

![Version](https://img.shields.io/badge/version-26.6-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## About Apex
Apex is a simple, cross-platform programming language designed for beginners and professional developers alike. It features a minimalistic indentation-based syntax, extensive built-in libraries for system interaction, file management, mathematics, data encoding, and more. It is distributed under the MIT License.

This extension brings full IDE support to VS Code, making it easier to write, read, and run Apex code.

## Features
### 🎨 Syntax Highlighting
Full syntax highlighting for:
- **Keywords**: `function`, `if`, `elif`, `else`, `for`, `import`, `and`, `or`, `not`, `break`, `continue`, `return`.
- **Literals**: Numbers (including scientific notation), strings, booleans (`true`, `false`).
- **Comments**: Line comments (`//`).
- **Operators**: Arithmetic (`+`, `-`, `*`, `/`, `%`), Comparison (`==`, `!=`, `<`, `>`, etc.), Logical.
- **Structures**: Function definitions, table literals `[]`, string interpolation `"{}"`.

### 🧠 Advanced IntelliSense & Autocompletion
Smart suggestions for:
- **Keywords**: Control flow and logical operators.
- **12 Standard Libraries**: `os`, `files`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `regex`, `codecs`.
- **Library Functions**: Auto-complete for hundreds of functions like `files.read()`, `sys.platform`, `random.randint()`, `codecs.json_write()`, etc.

### ℹ️ Hover Documentation
Hover over any keyword, module, or function to see instant documentation, syntax examples, and descriptions.
*Example: Hover over `for` to see range and iteration syntax, or `os` to see available system interaction functions.*

### ▶️ Run Code
Execute your current Apex file directly from VS Code.
- **Command Palette**: `Apex: Run Current File`
- **Keyboard Shortcut**: `F5` (when editing an `.apex` file)
- **Terminal**: Runs the file in a dedicated "Apex" terminal instance.

### 📑 Document Symbols
View the outline of your file in the VS Code Explorer. All defined functions are listed for easy navigation.

### 🛠️ Language Configuration
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
| :--- | :--- |
| **os** | System interaction |
| **files** | File system operations |
| **sys** | System information |
| **math** | Mathematical functions |
| **string** | String manipulation |
| **table** | Data structures |
| **ffi** | Foreign Function Interface |
| **random** | Random generation |
| **regex** | Regular expressions |
| **codecs** | Data encoding |

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

## License
This extension is licensed under the MIT License.

## Release Notes
### 26.6
- Initial release.
- Syntax highlighting for Apex language.
- IntelliSense for keywords and all 12 built-in libraries.
- Hover documentation for keywords and libraries.
- Command to run current file.
- Document symbol provider for functions.
- Support for scientific notation and bracket auto-closing.

---

**Enjoy coding in Apex!** 🚀