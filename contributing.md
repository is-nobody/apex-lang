# Contributing to Apex

[![GitHub](https://img.shields.io/badge/Platform-GitHub-green)](https://github.com/is-nobody/apex-lang)
[![Apex Version](https://img.shields.io/badge/Apex-lang-blue)](https://github.com/is-nobody/apex-lang)

![Last Commit](https://img.shields.io/github/last-commit/is-nobody/apex-lang)
![Issues](https://img.shields.io/github/issues/is-nobody/apex-lang)
![Pull Requests](https://img.shields.io/github/issues-pr/is-nobody/apex-lang)

Thank you for your interest in contributing to the Apex programming language! This guide will help you understand our development process and how to submit changes effectively. Whether you're fixing a typo or adding a major feature, your contribution matters.

Please note that by participating in this project, you agree to abide by our [Code of Conduct](code_of_conduct.md). We expect all contributors to help us maintain a welcoming and respectful community.

## Table of Contents
- [Development Setup](#development-setup)
- [Understanding the Project](#understanding-the-project)
- [Finding a Task](#finding-a-task)
- [Code Style Rules](#code-style-rules)
- [Making Changes](#making-changes)
- [Testing Your Changes](#testing-your-changes)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)
- [Getting Help](#getting-help)
- [What Makes a Good Contribution](#what-makes-a-good-contribution)

## License
By contributing to Apex, you agree that your contributions will be licensed under the [MIT License](license). You retain the copyright to your work, but grant the project a perpetual, worldwide, non-exclusive license to use, modify, and distribute your contributions as part of the project.

## Development Setup
### Prerequisites
- **Apex v26.06+** - The language itself (bootstrapping)
- **Git** - For version control

### Install Apex
If you haven't installed Apex yet:

1. Go to the [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
2. Navigate to the Download section
3. Select the appropriate file for your operating system
4. Download and run the installer

Verify the installation:

```bash
apex
```

Expected output: `Apex v26.06`.

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
| Location | Description |
|----------|-------------|
| [`main.apex`](main.apex) | Main entry point of the interpreter |
| [`source/`](source/) | Source code |
| [`source/core`](source/core/) | Core components |
| [`source/libraries/`](source/libraries/) | Built-in libraries |
| [`source/tests/`](source/tests/) | Test suite |
| [`resources/`](resources/) | Resources of the project |

### How the Pieces Connect
1. **main.apex** → reads source file
2. **Tokenizer** → converts source code into tokens
3. **Parser** → builds an Abstract Syntax Tree (AST) from tokens
4. **Interpreter** → executes the AST
5. **Built-in libraries** → provide OS, math, string, network and UI functions

## Finding a Task
Before making changes, check these places:

- **Issues** - Look for issues
- **Discussions** - Ask questions or suggest ideas
- **Roadmap** - Check `resources/roadmap.md` for planned features

### Issue Labels
| Label | Meaning |
|-------|---------|
| `bug` | Something isn't working correctly |
| `enhancement` | New feature or improvement |
| `documentation` | Docs need updating |
| `good-first-issue` | Great for new contributors |
| `help-wanted` | Maintainer seeking assistance |
| `testing` | Needs tests written |

## Code Style Rules
### Apex Code Style
Follow the existing style throughout the codebase.

Rules to follow:

1. **Indentation:** Use 4 spaces (no tabs)
2. **Function names:** Use `snake_case` (e.g., `get_user_data`)
3. **Variable names:** Use `snake_case` (e.g., `user_score`)
4. **Keywords:** Lowercase (`function`, `if`, `for`, `return`)
5. **Operators:** Add spaces around them (`a + b`, not `a+b`)
6. **Parentheses:** No space after function name (`greet(name)`, not `greet (name)`)
7. **Comments:** Explain *why* not *what* only

### Commit Messages
Write clear, descriptive commit messages in the present tense:

```
add division-by-zero check in calculator

fix parser crash on empty input

update installation guide for macOS
```

Keep commits focused: one logical change per commit.

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
- Start with `main.apex` to understand the execution flow
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
- Changes must not break existing tests

## Submitting Changes
### 1. Save Your Changes
```bash
git add .
git commit -m "description of your change"
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
1. Automated checks will run (tests, formatting)
2. Maintainers will review your code:
   - They may suggest changes - this is normal and helpful!
   - Address feedback by updating your branch
   - Push additional commits to the same branch
3. Once approved, your changes will be merged

### Review Timeline
- First review within 3-5 days
- Minor changes: faster
- Large features: may take longer

## Getting Help
### Before Asking
1. Check the `resources/docs/` folder
2. Search existing issues and discussions
3. Look at similar code in the project
4. Read the [Apex Reference Manual for Developers](resources/RM_fDevelopers.md)

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
### Small Wins (Great for Beginners)
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

### Best Practices
| Do | Don't |
|----|-------|
| One change per PR | Multiple unrelated fixes in one PR |
| Write clear commit messages | Vague messages like "fix" or "update" |
| Test your changes | Assume your code works without testing |
| Update documentation | Change behavior without updating docs |
| Ask questions when unsure | Guess and hope it's right |

## Troubleshooting Common Issues
### "Apex command not found"
- Reinstall Apex from [GitHub releases](https://github.com/is-nobody/apex-lang/releases)
- Check that Apex is in your PATH

### Tests failing after your change
- Run only the failing test to isolate the issue:

```bash
apex source/tests/specific_test.apex
```

- Use `os.output()` to debug values at key points in your code
- Compare your output against the expected behavior documented in the test
- Check that you haven't accidentally modified shared test utilities

### Merge conflicts
If your branch has fallen behind `main` and conflicts arise:

```bash
git checkout main
git pull upstream main
git checkout your-feature-branch
git rebase main
```

If conflicts occur during rebase:
1. Open the conflicting files and look for `<<<<<<<`, `=======`, `>>>>>>>` markers
2. Resolve each conflict manually
3. Stage the resolved files: `git add <filename>`
4. Continue the rebase: `git rebase --continue`

After rebase, force-push your updated branch:

```bash
git push --force-with-lease origin your-feature-branch
```

**Note:** Use `--force-with-lease` instead of `--force` — it's safer and won't overwrite changes others may have made to your branch.

If you're uncomfortable with rebasing, you can also merge instead:

```bash
git checkout main
git pull upstream main
git checkout your-feature-branch
git merge main
# Resolve conflicts, then:
git add .
git commit -m "merge main into feature branch"
git push origin your-feature-branch
```

## Thank You!
Your contributions make Apex better for everyone.

Remember:
- Everyone starts somewhere
- Questions are always welcome
- Your perspective is valuable
- You're making a difference

Happy coding!