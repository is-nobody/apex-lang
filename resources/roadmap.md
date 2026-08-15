**Apex Language Roadmap**
- Build for Android
- Build for iOS
- `ui` library (apex+html+css+system webview)
- `network` library
- `archive` library (zip, 7z, tar: pack/unpack)
- `\x` escape sequence support
- Package manager
- Language server (LSP)
- Debug mode
- Documentation `Apex as Embedded language`
- Resolve Cyclic Reference problem
- Concurrency model

**What's NOT on the roadmap**
- GC due to uncontrolled pauses
- JIT because the main CPU work is on the stdlib written in C
- AOT because good interpretation is better than bad compilation
- OOP because Apex must be simple
- Splitting a `number` to integer and float
- Exceptions because errors as values is faster and easy
- Decorators because they add hidden behavior
- Default parameters because functions must accept exactly the number of arguments they declare
- Lambdas because functions deserve a name for readability
- Pattern matching because `if/elif/else` are explicit
- Closures because functions should be pure and predictable
- Multiple return values because functions must return exactly one value
- Multiple assignment because it adds abuse which kills the style
- Operator overloading because `a + b` must always mean addition
- 'Optional' type hints because Apex is dynamically typed