# Contributing to Apex

[![GitHub](https://img.shields.io/badge/Platform-GitHub-green)](https://github.com/is-nobody/apex-lang)
[![Apex Version](https://img.shields.io/badge/Apex-Lang-blue)](https://github.com/is-nobody/apex-lang)

Thank you for your interest in contributing to the Apex programming language! This guide will help you understand our development process and how to submit changes effectively. Whether you're fixing a typo or adding a major feature, your contribution matters.

Please note that by participating in this project, you agree to abide by our [Code of Conduct](code_of_conduct.md). We expect all contributors to help us maintain a welcoming and respectful community.

## Table of Contents
- [License](#license)
- [Development Setup](#development-setup)
- [Understanding the Project](#understanding-the-project)
- [Finding a Task](#finding-a-task)
- [Code Style Rules](#code-style-rules)
- [Using AI Agents](#using-ai-agents)
- [Making Changes](#making-changes)
- [Testing Your Changes](#testing-your-changes)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)
- [Getting Help](#getting-help)
- [What Makes a Good Contribution](#what-makes-a-good-contribution)

## License
By contributing to Apex, you agree that your contributions will be licensed under the [MIT License](license). You still own your code. However, you allow the project to use, modify, and distribute it forever, anywhere in the world, without excluding anyone else from using it too.

## Development Setup
### Prerequisites
- **C Compiler** - For compiling the code
- **CMake** - For bundling the project
- **Git** - For version control

### Set Up Your Workspace
Create your own copy of the project:

1. Fork the repository on GitHub
2. Clone your fork locally:

```bash
git clone https://github.com/YOUR-USERNAME/apex-lang.git
cd apex-lang
```

3. Add the original repository as upstream (to keep your fork updated):

```bash
git remote add upstream https://github.com/is-nobody/apex-lang
```

4. Create a new branch for your work:

```bash
git checkout -b feature/your-feature-name
```

Branch naming examples:
- `feature/add-new-function`
- `fix/parser-error-handling`
- `docs/update-tables-section`
- `test/add-coverage`

## Understanding the Project
### Project Structure
| Location                                 | Description              |
|------------------------------------------|--------------------------|
| [`main.c`](main.c)                       | Main entry point         |
| [`source/`](source/)                     | Source code              |
| [`source/core`](source/core/)            | Core components          |
| [`source/libraries/`](source/libraries/) | Built-in libraries       |
| [`source/tests/`](source/tests/)         | Test suite               |
| [`resources/`](resources/)               | Resources of the project |

## Finding a Task
Before making changes, check these places:

- **Issues** - Look for issues
- **Discussions** - Ask questions or suggest ideas
- **Roadmap** - Check `resources/roadmap.md` for planned features

## Code Style Rules
### Apex Code Style
Follow the existing style throughout the codebase.

Rules to follow:

1. **Indentation:** Use 4 spaces (no tabs)
2. **Function names:** Use `snake_case` (e.g., `get_user_data`)
3. **Variable names:** Use `snake_case` (e.g., `user_score`)
4. **Operators:** Add spaces around them (`a + b`, not `a+b`)
5. **Parentheses:** No space after function name (`greet(name)`, not `greet (name)`)
6. **Comments:** Explain *why*, not only *what*

### Commit Messages
Write clear, descriptive commit messages. Each commit message must include a type prefix:

| Type       | Description                  | Example                                     |
|------------|------------------------------|---------------------------------------------|
| `feature`  | New feature or functionality | `feature: json library`                     |
| `fix`      | Bug fix                      | `fix: resolve parser crash on empty input`  |
| `docs`     | Documentation updates        | `docs: update installation guide for linux` |
| `refactor` | Code restructuring           | `refactor: simplify tokenizer loop logic`   |

Format:

```
<type>: <description in present tense>
```

Keep commits focused: one logical change per commit with its type prefix.

## Using AI Agents
### Project Policy
Apex permits the use of AI tools to accelerate development, but with restrictions.

### Usage Rules
1. **Code Review**: You bear full responsibility for the quality and correctness of AI-generated code
2. **Understanding**: You must understand and be able to explain every fragment of code you submit
3. **Testing**: AI-generated code must pass the same testing as manually written code

Remember: you are the author of the code, and AI is merely a tool.

**IT IS STRICTLY PROHIBITED** to use AI in any component of `source/core`, especially the VM. Reasons:

- **Performance-critical code paths**: AI-generated code **ALWAYS** contains redundant operations and unnecessary checks, which, when multiplied across millions of executions, leads to performance degradation by orders of magnitude.

- **Deep contextual understanding required**: The core VM relies on complex invariants, execution order guarantees, and subtle interaction mechanisms between components spanning thousands of lines of code. AI lacks understanding of these interdependencies even if it has seen the entire project.

## Making Changes
### Step-by-Step Process
1. Update your fork before starting:

```bash
git checkout main
git pull upstream main
git push origin main
git checkout -b feature/your-feature
```

2. Make focused changes - One feature or fix per branch
3. Test your changes as you go (see Testing section)
4. Update documentation if you change user-facing behavior
5. Commit your changes with a clear message

### Code Navigation Tips
- Start with `main.c` to understand the execution flow
- Look at similar features before implementing new ones
- Use `os.output()` for debugging (remove before committing)
- Check existing tests to understand function behavior

## Testing Your Changes
### Running Tests
```bash
apex source/tests/run_all_tests.apex
```

### Writing Tests
When adding features or fixing bugs, include or update tests.

**Test requirements:**
- All new features must include tests
- Bug fixes must include regression tests
- Changes should not break existing tests

## Submitting Changes
### 1. Save Your Changes
```bash
git add .
git commit -m "fix: description of your change"
git push origin feature/your-feature-name
```

### 2. Create a Pull Request
Go to your fork on GitHub and click **"Compare & pull request"**

Fill in the PR template completely:

```markdown
## Description
What does this PR do?

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Refactor

## Testing
How did you test this?

## Related Issues
Closes #(issue number)
```

## Review Process
Maintainers will review your code:

- They may suggest changes — this is normal and helpful!
- Address feedback by updating your branch
- Push additional commits to the same branch

Once approved, your changes will be merged

### Review Timeline
- First review within 3-5 days
- Minor changes: faster
- Large features: may take longer

## Getting Help
### Before Asking
1. Check the `resources/` folder
2. Search existing issues and discussions
3. Look at similar code in the project
4. Read the [Apex Reference Manual for Developers](resources/RM_fDevelopers.md)
5. Check the [Roadmap](resources/roadmap.md)

### When You Need Help
1. **Ask in Discussions** - For general questions
2. **Comment on Issues** - For specific problems
3. **Tag maintainers** - If directed in the issue

### Communication Tips
- Be clear about what you're trying to do
- Share what you've already tried
- Include error messages or screenshots
- Be patient and respectful

## What Makes a Good Contribution
### Small Contributions
- Fix typos in documentation
- Improve error messages
- Add helpful comments
- Update outdated information
- Add test coverage for existing features

### Bigger Contributions
- Fix bugs
- Add new built-in library functions
- Improve interpreter performance
- Add better documentation and examples
- Implement missing language features

### Thank You!
Your contributions make Apex better for everyone.

Remember:
- Everyone starts somewhere
- Questions are always welcome
- Your perspective is valuable
- You're making a difference

Happy coding!