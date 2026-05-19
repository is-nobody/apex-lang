# Apex Language Roadmap

## Mobile & VM Foundation
| Task | Description |
|------|-------------|
| **AST to Bytecode** | Convert Abstract Syntax Tree into portable bytecode |
| **Bytecode VM** | Write a stack-based VM to execute bytecode efficiently |
| **Mobile runtime** | Port the VM to iOS (Swift/Kotlin) and Android (NDK) |
| **REPL on mobile** | Run Apex code interactively on phones/tablets |

## Compiler & Binary Output
| Task | Description |
|------|-------------|
| **Native binary compiler** | Compile Apex directly to machine code (via LLVM or custom backend) |
| **Minimal runtime** | Reduce binary size (< 2 MB) |
| **Static linking** | Bundle only used libraries |
| **Cross-compilation** | Build for Windows/macOS/Linux from any platform |

## UI Library (First Class)
| Task | Description |
|------|-------------|
| **Declarative UI syntax** | `ui.window`, `ui.button`, `ui.text`, `ui.input` |
| **Event handling** | `on_click`, `on_change`, `on_close` |
| **Layout system** | Flexbox-like (row, column, grid) |
| **Native backends** | Win32 (Windows), Cocoa (macOS), GTK (Linux) |
| **Simple components** | `ui.show()`, `ui.run()` |

### Tooling & Package Manager
| Task | Description |
|------|-------------|
| **Apex Package Manager (apm)** | `apm install`, `apm publish` |
| **Package registry** | Simple GitHub-based registry |
| **Formatter** | `apex fmt` — standard code style |
| **LSP (Language Server)** | Code completion, go-to-definition, hover |
| **VS Code extension** | Syntax highlighting + LSP integration |