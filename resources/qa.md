# Apex language QA
- **Q**: Does Apex guarantee compatibility between versions?  
- **A**: Yes, but we don't promise 100% compatibility. In any case, breaking changes between versions should be minimal.

---

- **Q**: Is there a plan to add JIT?  
- **A**: No, because the heavy CPU work is done by the standard library, which is written in C. JIT won't provide any benefit, it will only bloat the project.

---

- **Q**: Will there be AOT compilation to machine code?  
- **A**: No, because the primary function of Apex is to stitch fast C components together and provide a developer-friendly experience. Also, it's better to have a good interpretation than a bad compilation. The `apex build` command already bundles bytecode with the interpreter into a single, dependency-free binary

---

- **Q**: Will Apex on mobile produce standalone APK/IPA?  
- **A**: Yes! To do this, you will need to use a UI library. This feature will be available later, we have different tasks now, but we will try to do this as soon as possible.

---

- **Q**: Why is the entire project independent, even the cryptography?  
- **A**: Complete control over the code. If you know a bug has occurred, you will be certain it is in your code. Integrating OpenSSL would add 1.2 MB to the binary and necessitate the inclusion of an external license in the project.

---

- **Q**: Will Apex have `try`/`catch`? And why errors as values?  
- **A**: There won't be a `try`/`catch`. When we were considering how to make the error-handling system simple and fast, "errors as values" was the clear winner.

---

- **Q**: How is the VM speed achieved?  
- **A**: Many things, but the most important ones: computed gotos, nan-boxing, register locals, string interning, dual array/hash tables, compile-time specialized opcodes. That might have sounded unclear, but it’s a normal reaction!

---

- **Q**: Why `output` in `os` library, not built-in?
- **A**: This provides modularity and clarity: why is `output` built-in, but `args` is in `os`? So why there are three built-in functions: `type`, `number`, `string`? Because they provide fundamental handling of the language's types.

---

- **Q**: Why only `number` type instead of `integer` and `float`?  
- **A**: Because normal people don't care about the difference between `3` and `3.0`. They just want math to work. The machine can figure out the representation.

---

- **Q**: Why is everything 1-indexed instead of 0-indexed?  
- **A**: Because normal people count "first, second, third", not "zeroth, first, second". The machine can subtract 1 internally. You shouldn't have to.

---

- **Q**: Why do math operators work only on numbers?  
- **A**: Because if `+` is available for strings, then it seems all the math should work too. We don't need it.

---

- **Q**: Why only interpolation for strings?  
- **A**: Because using the `+` operator to combine `Hello ` and `World` looks unreadable at scale and bloats the code.

---

- **Q**: Why no `truthy/falsy` values in comparisons?  
- **A**: Good luck reading someone else's code at 3 AM! Implicit truthiness always raises questions — is `""` true? Is `[]` false?

---

- **Q**: Will there be a code formatter?  
- **A**: No, because the parser itself enforces correct code style. You cannot write valid and unreadable code in Apex.

---

- **Q**: Are there plans for shorthand assignment operators?
- **A**: No. "Assignment operators" are an unnecessary feature that encourages writing code like `x += y * z`, which obscures the order of operations and the assignment itself. 