**Apex Language Roadmap**
- Build for Android/iOS
- `ui` library (apex+html+css+system webview)
- `\x` escape sequence support
- Package manager
- Language server (LSP)
- Documentation `Apex as Embedded language`
- Resolve Cyclic Reference problem
- Concurrency model

**What's NOT on the roadmap**
- GC due to uncontrolled pauses
- JIT because the main CPU work is on the stdlib written in C
- OOP because hidden state makes behavior unpredictable
- Decorators because they add hidden behavior
- Default parameters because functions must accept exactly the number of arguments they declare
- Lambdas because functions deserve a name for readability
- Closures because functions should be pure and predictable
- Multiple return values because functions must return exactly one value
- Multiple assignment because it kills the style
- Operator overloading because `a + b` must always mean addition
- Optional type hints because Apex is dynamically typed