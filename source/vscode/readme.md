# Apex for Visual Studio Code
A Visual Studio Code extension providing syntax highlighting, code snippets, IntelliSense, and execution support for the **Apex** programming language.

![Version](https://img.shields.io/badge/Version-26.07-blue)
![License](https://img.shields.io/badge/License-MIT-green)

## About Apex
Apex is a programming language built for speed, power, simplicity, clarity, readability, and modernity. It's under the MIT License. Created by one person.

This extension adds support for the Apex language to VS Code.

## Features
### Syntax Highlighting
Full syntax highlighting for:

- **Keywords**: `function`, `if`, `elif`, `else`, `for`, `import`, `and`, `or`, `not`, `break`, `continue`, `return`.
- **Literals**: Numbers (including scientific notation), strings, booleans (`true`, `false`).
- **Comments**: Line comments (`//`).
- **Operators**: Arithmetic (`+`, `-`, `*`, `/`, `%`), Comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), Logical (`not`, `and`, `or`).
- **Structures**: Function definitions, table literals `[]`, string interpolation `"{}"`.

### Advanced IntelliSense & Autocompletion
Smart suggestions for:

- **Keywords**: Control flow and logical operators.
- **9 Standard Libraries**: `os`, `sys`, `math`, `string`, `table`, `ffi`, `random`, `codecs`, `regex`.
- **Library Functions**: Auto-complete for hundreds of functions like `os.read()`, `sys.platform`, `random.randint()`, `codecs.json_write()`, etc.

### Run Code
Execute your current Apex file directly from VS Code:

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

## Release Notes
### 26.07 (July 31, 2026)
- Syntax highlighting for shebang (`#!`) at the beginning of scripts.
- Indentation-based folding for code blocks.
- Syntax highlighting for the new `none` data type.
- Added support for single-quoted strings (`'...'`) with proper syntax highlighting and auto-closing.
- Trimmed standard library descriptions — removed inline function listings.

### 26.06 (June 30, 2026)
- Initial release.
- Syntax highlighting for Apex language.
- IntelliSense for keywords and all 8 built-in libraries.
- Hover documentation for keywords and libraries.
- Command to run current file.
- Document symbol provider for functions.
- Support for scientific notation and bracket auto-closing.