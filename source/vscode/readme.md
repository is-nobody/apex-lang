# Apex Language Support
A Visual Studio Code extension providing syntax highlighting, code snippets, IntelliSense, and execution support for the **Apex** programming language.

![Version](https://img.shields.io/badge/version-26.6-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## About Apex
Apex is a simple, cross-platform programming language designed for beginners and professional developers alike. It features a minimalistic syntax, built-in libraries for OS interaction, math, string manipulation, and table management, and is distributed under the MIT License.

This extension brings full IDE support to VS Code, making it easier to write, read, and run Apex code.

## Features

### 🎨 Syntax Highlighting
Full syntax highlighting for:
- Keywords (`function`, `if`, `elif`, `else`, `for`, `import`, etc.)
- Data types and literals (`true`, `false`, numbers, strings)
- Comments (`//`)
- Operators (Arithmetic, Comparison, Logical)
- Function definitions and calls
- Library calls (`os.output`, `math.sqrt`, etc.)

### IntelliSense & Autocompletion
Smart suggestions for:
- **Keywords**: Control flow and logical operators.
- **Libraries**: `os`, `math`, `string`, `table`.
- **Library Functions**: Auto-complete for functions like `os.input()`, `math.abs()`, `string.split()`, etc.

### ℹ️ Hover Documentation
Hover over any keyword or library function to see instant documentation, syntax examples, and descriptions.
*Example: Hover over `for` to see loop syntax, or `os` to see available system functions.*

### ▶️ Run Code
Execute your current Apex file directly from VS Code.
- **Command Palette**: `Apex: Run Current File`
- **Keyboard Shortcut**: `Ctrl+Shift+R` (when editing an `.apex` file)
- **Status Bar**: Click the **$(play) Run Apex** button in the bottom right status bar.

Output is displayed in the "Apex" output channel.

### 📑 Document Symbols
View the outline of your file in the VS Code Explorer. All defined functions are listed for easy navigation.

## Installation
1. Install the [Apex Interpreter](https://github.com/is-nobody/apex-lang/releases) on your system.
   - Ensure the `apex` command is available in your system's PATH.
2. Install this extension from the VS Code Marketplace or by installing the `.vsix` file.
3. Open any file with the `.apex` extension.

## Requirements

- **VS Code**: Version 1.60.0 or higher.
- **Apex Runtime**: The Apex interpreter must be installed and accessible via the terminal command `apex`.

## Extension Settings

This extension contributes the following settings:

*None currently. The extension assumes the `apex` command is in your system PATH.*

## Known Issues

- Ensure the Apex interpreter is correctly installed if the "Run File" command fails.
- Syntax highlighting may require a reload of VS Code after installation.

## Release Notes

### 26.6
- Initial release.
- Syntax highlighting for Apex language.
- IntelliSense for keywords and built-in libraries.
- Hover documentation for keywords and libraries.
- Code snippets for common structures.
- Command to run current file.
- Document symbol provider for functions.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This extension is licensed under the MIT License.

---

**Enjoy coding in Apex!** 🚀