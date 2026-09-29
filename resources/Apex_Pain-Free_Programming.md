# Apex: Pain-Free Programming
## Table of Contents
### Introduction
- [Preface](#preface)
- [What is a "programming language"?](#what-is-a-programming-language)
- [A bit of history about Apex](#a-bit-of-history-about-apex)
- [Preparation for development](#preparation-for-development)
  - [Installing the Apex Language](#installing-the-apex-language)
  - [Installing the Apex Code](#installing-the-apex-code)
- [First Program](#first-program)

### Variables & Data Types
- [Numbers](#numbers)
  - [No Distinctions, No Friction](#no-distinctions-no-friction)
  - [Whole Numbers](#whole-numbers)
  - [Decimal Numbers](#decimal-numbers)
  - [Positive and Negative](#positive-and-negative)
- [Strings](#strings)
  - [Creating Strings](#creating-strings)
  - [Strings Are Not Numbers](#strings-are-not-numbers)
  - [Escape Sequences](#escape-sequences)
  - [Multiline Strings](#multiline-strings)
  - [String Interpolation](#string-interpolation)
  - [Curly Braces in Strings](#curly-braces-in-strings)
- [Booleans](#booleans)
  - [Why Booleans Exist](#why-booleans-exist)
  - [Creating Booleans](#creating-booleans)
  - [Naming Boolean Variables](#naming-boolean-variables)
  - [Booleans Are Not Strings](#booleans-are-not-strings)
  - [Booleans as Data](#booleans-as-data)
- [Tables](#tables)
  - [Creating an Empty Table](#creating-an-empty-table)
  - [Creating a Table with Values](#creating-a-table-with-values)
  - [Ordered Lists](#ordered-lists)
  - [Adding and Changing Items](#adding-and-changing-items)
  - [Key-Value Pairs](#key-value-pairs)
  - [Adding and Changing Key-Value Pairs](#adding-and-changing-key-value-pairs)
  - [Accessing a Key That Doesn't Exist](#accessing-a-key-that-doesnt-exist)
  - [Mixed Tables](#mixed-tables)
  - [Tables Inside Tables](#tables-inside-tables)
  - [A Quick Word on Positions vs. Keys](#a-quick-word-on-positions-vs-keys)
- [None](#none)
  - [Not an Empty String or Table](#not-an-empty-string-or-table)
  - [The Role of None](#the-role-of-none)
- [Constant](#constant)
  - [The Problem Constants Solve](#the-problem-constants-solve)
  - [What Constant Does](#what-constant-does)
  - [Constants and Data Types](#constants-and-data-types)
  - [A Note on Naming](#a-note-on-naming)
- [Built-in Functions](#built-in-functions)
  - [Three Essential Built-ins](#three-essential-built-ins)
  - [type(): Checking What Something Is](#type-checking-what-something-is)
  - [number(): Converting to a Number](#number-converting-to-a-number)
  - [string(): Converting to a String](#string-converting-to-a-string)
  - [Built-ins Are Functions Like Any Other](#built-ins-are-functions-like-any-other)

### Operators
- [Arithmetic Operators](#arithmetic-operators)
  - [What Is an Operator?](#what-is-an-operator)
  - [The Five Arithmetic Operators](#the-five-arithmetic-operators)
  - [Addition](#addition)
  - [Subtraction](#subtraction)
  - [Multiplication](#multiplication)
  - [Division](#division)
  - [Modulo](#modulo)
  - [Operator Precedence](#operator-precedence)
  - [Using Parentheses to Control Order](#using-parentheses-to-control-order)
  - [Combining Operators with Variables](#combining-operators-with-variables)
  - [Arithmetic Only Works with Numbers](#arithmetic-only-works-with-numbers)
  - [Whole Numbers and Decimals Together](#whole-numbers-and-decimals-together)
- [Comparison Operators](#section)
  - [What Comparison Operators Do](#what-comparison-operators-do)
  - [Equal To](#equal-to)
  - [Not Equal To](#not-equal-to)
  - [Less Than and Greater Than](#less-than-and-greater-than)
  - [Less Than or Equal To and Greater Than or Equal To](#less-than-or-equal-to-and-greater-than-or-equal-to)
  - [Comparison Results Are Booleans](#comparison-results-are-booleans)
  - [Comparisons with Variables on Both Sides](#comparisons-with-variables-on-both-sides)
  - [Comparison Only Works with Compatible Types](#comparison-only-works-with-compatible-types)
  - [Operator Precedence](#operator-precedence)
  - [Chaining Comparisons](#chaining-comparisons)
- [Logical Operators](#logical-operators)
  - [The Three Logical Operators](#the-three-logical-operators)
  - [The AND Operator](#the-and-operator)
  - [The OR Operator](#the-or-operator)
  - [The NOT Operator](#the-not-operator)
  - [Combining Logical Operators](#combining-logical-operators)

### If Statements
- [If Statement](#if-statement)
  - [A Note on the Block](#a-note-on-the-block)
  - [Multiple Conditions](#multiple-conditions)
- [Else-If Statement](#else-if-statement)
  - [Order Matters](#order-matters)
  - [Each Branch Is Its Own Scope](#each-branch-is-its-own-scope)
- [Else Statement](#else-statement)
  - [When to Use Else](#when-to-use-else)
  - [A Common Pattern: Validation](#a-common-pattern-validation)
  - [No Else Needed for Simple Cases](#no-else-needed-for-simple-cases)
- [Ternary Expression](#ternary-expression)
  - [Ternary vs. If Statement](#ternary-vs-if-statement)
  - [Cannot Chain Ternaries](#cannot-chain-ternaries)
  - [A Word on Apex's Ternary Order](#a-word-on-apexs-ternary-order)
  - [Boolean Conditions Need Explicit Comparison](#boolean-conditions-need-explicit-comparison)
  - [A Practical Example](#a-practical-example)

### Match / Case
- [Match Statement](#match-statement)
  - [A Note on the Block](#a-note-on-the-block)
  - [Cases Are Checked from Top to Bottom](#cases-are-checked-from-top-to-bottom)
- [Case Patterns](#case-patterns)
  - [Patterns Must Match the Subject's Type](#patterns-must-match-the-subjects-type)
  - [Constants Only, No Variables or Expressions](#constants-only-no-variables-or-expressions)
  - [Empty String and Zero Are Valid Patterns](#empty-string-and-zero-are-valid-patterns)
- [Default Case](#default-case)
  - [The Default Case Must Be Last](#the-default-case-must-be-last)
  - [Only One Default Case](#only-one-default-case)
  - [When to Use a Default Case](#when-to-use-a-default-case)
- [Rules and Restrictions](#rules-and-restrictions)
  - [1. Subject Type](#1-subject-type)
  - [2. Pattern Type](#2-pattern-type)
  - [3. Constants Only](#3-constants-only)
  - [4. Order Matters](#4-order-matters)
  - [5. Default Case](#5-default-case)
  - [6. Scope](#6-scope)
  - [7. Not an Expression](#7-not-an-expression)
  - [8. No Tables](#8-no-tables)
  - [9. No Range Patterns](#9-no-range-patterns)
  - [10. No Compound Patterns](#10-no-compound-patterns)
  - [11. No Fallthrough](#11-no-fallthrough)
  - [12. Empty Case Bodies Are Allowed](#12-empty-case-bodies-are-allowed)
- [Putting It Together](#putting-it-together)
  - [When to Use Match vs. If](#when-to-use-match-vs-if)
  - [A Reminder About Comparison Semantics](#a-reminder-about-comparison-semantics)

### For Loops
- [For Counter](#for-counter)
  - [Counting Down with a Step](#counting-down-with-a-step)
  - [The Loop Variable Is a Number](#the-loop-variable-is-a-number)
  - [Using a Decimal Start or End](#using-a-decimal-start-or-end)
  - [The Loop Variable Cannot Be Modified Inside the Loop](#the-loop-variable-cannot-be-modified-inside-the-loop)
  - [Nested Counter Loops](#nested-counter-loops)
- [For Table Iteration](#for-table-iteration)
  - [Iterating Over Key-Value Tables](#iterating-over-key-value-tables)
  - [Iterating Over Mixed Tables](#iterating-over-mixed-tables)
  - [Iterating Over an Empty Table](#iterating-over-an-empty-table)
  - [The Loop Variable Holds a Reference](#the-loop-variable-holds-a-reference)
  - [You Cannot Iterate Over Strings](#you-cannot-iterate-over-strings)
- [For Condition](#for-condition)
  - [The Condition Can Be Any Boolean Expression](#the-condition-can-be-any-boolean-expression)
  - [The Condition Is Re-Evaluated Before Each Iteration](#the-condition-is-re-evaluated-before-each-iteration)
  - [Counting Down](#counting-down)
  - [A Warning About Infinite Loops](#a-warning-about-infinite-loops)
  - [The Condition Must Be Boolean](#the-condition-must-be-boolean)
  - [Zero Iterations](#zero-iterations)
- [Break](#break)
  - [Break in Counter Loops](#break-in-counter-loops)
  - [Break in Condition Loops](#break-in-condition-loops)
  - [Break Only Exits the Innermost Loop](#break-only-exits-the-innermost-loop)
- [Continue](#continue)
  - [Continue in Table Iteration](#continue-in-table-iteration)
  - [Continue Only Affects the Innermost Loop](#continue-only-affects-the-innermost-loop)
  - [Continue vs. Break](#continue-vs-break)
  - [A Common Pattern: Filtering](#a-common-pattern-filtering)
  - [When to Use Continue](#when-to-use-continue)

### Functions
- [Why Functions Exist](#why-functions-exist)
- [Function Statement](#function-statement)
  - [Naming Functions](#naming-functions)
  - [Functions Are Values](#functions-are-values)
- [Parameters](#parameters)
  - [Parameters Are Local](#parameters-are-local)
  - [Parameters Must Be Provided](#parameters-must-be-provided)
  - [Parameters Are Copies](#parameters-are-copies)
- [Return Value](#return-value)
  - [Returning Early](#returning-early)
  - [Functions Without a Return](#functions-without-a-return)
  - [Return Ends the Function](#return-ends-the-function)
  - [A Function Returns Exactly One Value](#a-function-returns-exactly-one-value)
- [Call](#call)
- [Scope and Blocks](#scope-and-blocks)
  - [Functions Inside Functions](#functions-inside-functions)
  - [Blocks Inside Functions](#blocks-inside-functions)
- [Early Return](#early-return)
- [Recursion](#recursion)

### Async / Await
- [Why Async Exists](#why-async-exists)
- [What Is a Future](#what-is-a-future)
- [Async Function](#async-function)
  - [Why Mark a Function Async?](#why-mark-a-function-async)
  - [Async Functions Return Futures](#async-functions-return-futures)
- [Await](#await)
  - [Await Does Not Block Everything](#await-does-not-block-everything)
  - [Await Always Gives a Value](#await-always-gives-a-value)
- [Where Await Can Be Used](#where-await-can-be-used)
  - [Top-Level Await](#top-level-await)
  - [Await Inside Async Functions](#await-inside-async-functions)
- [What Can Be Awaited](#what-can-be-awaited)
  - [Awaiting Builtins](#awaiting-builtins)
  - [Awaiting Without Awaiting](#awaiting-without-awaiting)
  - [When You Don't Need to Await](#when-you-dont-need-to-await)
- [Running in the Background](#running-in-the-background)
  - [Example: A Chain of Awaits](#example-a-chain-of-awaits)
  - [The Scheduler](#the-scheduler)

### Imports
- [Why Imports Exist](#why-imports-exist)
- [Importing an Entire File](#importing-an-entire-file)
  - [The `.apex` Extension Is Required](#the-apex-extension-is-required)
  - [Where Does Apex Look for the File?](#where-does-apex-look-for-the-file)
- [What Gets Imported](#what-gets-imported)
  - [Modules Run Once](#modules-run-once)
- [Importing from Sub-folders](#importing-from-sub-folders)
  - [A Caution About Names](#a-caution-about-names)
- [Importing from One Sub-folder into Another](#importing-from-one-sub-folder-into-another)
- [Aliasing](#aliasing)
  - [Aliases Must Be Unique](#aliases-must-be-unique)
  - [Aliases Are Only for User Modules](#aliases-are-only-for-user-modules)
- [Built-in Modules Are Different](#built-in-modules-are-different)
  - [Built-in Modules Do Not End with `.apex`](#built-in-modules-do-not-end-with-apex)
  - [Accessing Built-in Contents](#accessing-built-in-contents)
  - [One Built-in Module at a Time](#one-built-in-module-at-a-time)
- [Rules and Restrictions](#rules-and-restrictions)
  - [1. Import at the Top](#1-import-at-the-top)
  - [2. Import Paths Are Relative to the Main File](#2-import-paths-are-relative-to-the-main-file)
  - [3. `.apex` for User Files, Nothing for Built-in Modules](#3-apex-for-user-files-nothing-for-built-in-modules)
  - [4. No Aliasing Built-in Modules](#4-no-aliasing-built-in-modules)
  - [5. Aliases Must Be Unique and Valid](#5-aliases-must-be-unique-and-valid)
  - [6. No Multiple Modules Per Import](#6-no-multiple-modules-per-import)
  - [7. Only Top-Level Definitions Are Imported](#7-only-top-level-definitions-are-imported)
  - [8. Module Names Come from File Names](#8-module-names-come-from-file-names)
  - [9. Files Must Exist and Be Readable](#9-files-must-exist-and-be-readable)
  - [10. Built-in Modules Must Be Imported Before Use](#10-built-in-modules-must-be-imported-before-use)

### Conclusion
- [Conclusion](#conclusion)

## Introduction
### Preface
Programming doesn't have to be painful.

That might sound like an odd way to start a book about a programming language. After all, many people who try to learn programming find it frustrating, confusing, and just plain hard. They wrestle with strange symbols, cryptic error messages, and rules that seem designed to trip them up. They spend more time fighting the language than solving problems.

Apex was created to change that.

This book is for anyone who wants to learn to program, whether you've never written a line of code in your life or you've used other languages and walked away feeling exhausted.

The philosophy of Apex is simple: **programming should feel natural**. The language should work the way you think, not the other way around. It should be readable, predictable, and forgiving. It should let you express your ideas clearly without forcing you to memorize arcane rules.

Throughout this book, you'll notice a recurring theme: we'll explain not just *how* to do something in Apex, but *why* it works that way. Understanding the reasoning behind a design choice makes it easier to remember and apply. You won't just be memorizing syntax—you'll be building a mental model of how programs work.

This book assumes no prior programming experience. Every concept is introduced from scratch, with plenty of examples and explanations. If you already know another language, you'll find that Apex's simplicity makes it easy to pick up, and the explanations of *why* things work the way they do will deepen your understanding of programming in general.

Let's begin.

### What is a "programming language"?
Before we dive into Apex specifically, let's take a step back and answer a more fundamental question: what exactly is a programming language?

At its core, a **programming language** is a way to give instructions to a computer. It's a formal system of symbols and rules that lets you describe what you want the computer to do. When you write a program, you're essentially writing a detailed set of directions—like a recipe—that the computer will follow step by step.

But why do we need special languages for this? Why can't we just tell the computer in plain English what we want?

The answer is that computers are incredibly literal and incredibly fast, but they're also incredibly dumb. They can only do a handful of very simple things: add numbers, compare values, move data around, and make basic decisions. They have no intuition, no common sense, and no ability to guess what you meant if you're ambiguous. If you told a computer to "make me a sandwich," it would have no idea where to start.

A programming language bridges this gap. It provides a structured way to express your intentions that's precise enough for the computer to understand, but also readable enough for humans to work with. It's a compromise between the rigid binary that the machine actually executes (ones and zeros) and the flexible, ambiguous language we use with each other.

Here's a useful analogy: think of a programming language as a set of LEGO bricks. Each brick is a simple, well-defined piece. On its own, a single brick doesn't do much. But when you combine bricks in the right way, you can build anything—a house, a car, a spaceship. The bricks give you structure and predictability, but they also give you freedom to create.

Different programming languages provide different sets of bricks. Some give you thousands of specialized pieces and expect you to learn them all. Others give you a few basic pieces and let you combine them in creative ways. Apex falls into the second category. It gives you a small, focused set of tools and trusts you to use them well.

Every programming language has two audiences:

1. **The computer**, which needs precise, unambiguous instructions it can execute.
2. **The programmer** (you), who needs to read, write, and modify those instructions.

A well-designed language serves both audiences. It's precise enough for the machine, but clear enough for humans. This is sometimes called the "readability" of a language, and it's one of the most important qualities a language can have. After all, you'll spend far more time *reading* code than writing it—your own code from last week, your teammate's code, code from an open-source project. A language that's hard to read is a language that's hard to maintain.

Apex was designed with both audiences in mind. It's simple enough to learn quickly, but powerful enough to build real software. It's precise enough for the computer, but readable enough that you can come back to your code months later and still understand what it does.

### A bit of history about Apex
Apex began with a simple frustration: programming languages had become too complicated.

The creator of Apex had worked with many languages over the years—languages that promised power and flexibility but delivered complexity and confusion. Languages that required pages of configuration before you could write a single line of code. Languages that had dozens of ways to do the same thing, none of them obviously better than the others. Languages where simple ideas were expressed in obscure syntax, and where the rules seemed designed to catch you off guard.

The breaking point came when trying to teach someone to program. The person was smart and motivated, but every step of the way, the language itself got in the way. They'd write something that seemed perfectly reasonable, only to be told it was wrong for a reason that made no sense. They'd spend hours debugging a missing semicolon or a mismatched bracket. They'd ask "why does it work this way?" and the only answer was "because that's how it's always been."

That's when the idea for Apex was born: what if a programming language was actually designed to be *helpful*? What if it was designed around the way humans think, rather than the way machines work? What if it eliminated all the unnecessary complexity and kept only what was essential?

Apex is the result of that idea. It's a language built on a few core principles:

1. **Simplicity**: The language should be small enough to learn completely. There should be one obvious way to do things, not five.

2. **Clarity**: Code should read like a description of what it does. If you have to puzzle over what a line of code means, the language has failed.

3. **Predictability**: The same code should always do the same thing. There should be no hidden behavior, no magic, no surprises.

4. **Performance**: Simplicity shouldn't come at the cost of speed. A language that's easy to write but too slow to use isn't helpful—it's just a different kind of painful.

The language has been in development for several months, refined through use and feedback. It's not the work of a large corporation or a committee—it's the work of people who genuinely care about making programming accessible and enjoyable. Every design decision has been made with the user in mind, asking "does this make programming easier or harder?" and choosing the path that makes it easier.

### Preparation for development
Before you can start writing Apex programs, you need to set up your development environment. Don't worry—this is much simpler than it sounds. You need two things:

1. The Apex interpreter, which reads your code and runs it.
2. A place to write your code (a text editor or IDE).

Let's take care of both.

#### Installing the Apex Language
The Apex interpreter is the program that reads your Apex code and executes it. You can't run Apex programs without it, so this is the first thing we need to install.

**Step 1: Download the interpreter.**
Go to the Apex releases page on GitHub: [https://github.com/is-nobody/apex-lang/releases](https://github.com/is-nobody/apex-lang/releases)

You'll see a list of releases. Find the most recent one and download the file that matches your operating system:

- **Windows**: Look for a file ending in `.exe`
- **macOS**: Look for a file with `macos` in the name
- **Linux**: Look for a file with `linux` in the name

**Step 2: Place the interpreter somewhere convenient.**
Once you've downloaded the file, move it to a location where you can easily access it from the command line. On Windows, you might create a folder like `C:\apex\` and put the file there. On macOS or Linux, you might put it in `/usr/local/bin/` or `~/bin/`.

**Step 3: Add Apex to your system's PATH (optional but recommended).**
If you want to be able to run `apex` from any directory without typing the full path, you need to add its location to your system's PATH environment variable. The exact steps depend on your operating system:

- **Windows**: Search for "Environment Variables" in the Start menu, edit the `Path` variable, and add the folder where you put the Apex interpreter.
- **macOS/Linux**: Edit your shell configuration file (like `.bashrc` or `.zshrc`) and add a line like `export PATH="$PATH:/path/to/apex/folder"`.

If you're not sure how to do this, don't worry—you can always run Apex by typing the full path to the interpreter.

**Step 4: Verify the installation.**
Open a terminal (Command Prompt on Windows, Terminal on macOS/Linux) and type:

```
apex version
```

If everything is set up correctly, you should see output like:

```
Apex 26.09 [GCC 15.2.0] on Linux x86-64
```

The exact details will vary depending on your system, but if you see "Apex" followed by a version number, you're good to go.

If you see an error message like "command not found" or "apex is not recognized," it means the interpreter isn't in your PATH. Either add it to your PATH as described above, or use the full path to the interpreter when running commands.

#### Installing the Apex Code
Now that you have the interpreter installed, you need a place to write your code. Technically, you could use any text editor—even Notepad on Windows or TextEdit on macOS. But a good code editor will make your life much easier by providing features like syntax highlighting, auto-completion, and error detection.

For this book, we'll assume you're using VS Code, since it's the most beginner-friendly option. Here's how to set it up:

**Step 1: Download and install VS Code.**
Go to [https://code.visualstudio.com/](https://code.visualstudio.com/) and download the version for your operating system. Follow the installation instructions.

**Step 2: Install the Apex extension.**
Open VS Code and click on the Extensions icon in the sidebar (it looks like a square puzzle piece). In the search box, type `apex-lang` and download the extension published by `is-nobody`. Click **Install**.

Alternatively, launch VS Code Quick Open (`Ctrl+P`), paste the following command, and press Enter:

```
ext install is-nobody.apex-lang
```

This extension provides:
- Syntax highlighting (your code will be colored to make it easier to read)
- Auto-completion (the editor will suggest what to type next)
- Hover documentation (hover over a function to see what it does)
- A command to run your code directly from the editor

**Step 3: Create a workspace.**
Create a folder somewhere on your computer where you'll keep your Apex projects. For example, you might create a folder called `apex-projects` in your home directory. Open this folder in VS Code using File > Open Folder.

That's it! You're ready to start writing Apex code.

### First Program
Now for the moment you've been waiting for: writing your first Apex program.

By tradition, the first program in any language is "Hello, World!"—a program that simply prints those words to the screen. It's a simple task, but it introduces several fundamental concepts: how to write a program, how to run it, and how to see output.

Let's create the program step by step.

**Step 1: Create a new file.**
In VS Code, create a new file by clicking File > New File. Then save it with the name `hello.apex`. The `.apex` extension tells VS Code that this is an Apex file, so it will apply the right syntax highlighting.

**Step 2: Write the program.**
Type the following code into the file:

```apex
import os

os.output("Hello, World!")
```

That's it—two lines of code. Let's break down what each line does.

The first line, `import os`, tells Apex that we want to use the `os` module. A **module** is a collection of related functions and values. The `os` module contains functions for interacting with the operating system—reading files, writing to the screen, getting the current directory, and so on.

The second line, `os.output("Hello, World!")`, calls the `output` function from the `os` module. This function takes a value (in this case, the string `"Hello, World!"`) and prints it to the screen, followed by a newline.

Notice the syntax:
- `os.output` is the function's name. The dot `.` separates the module name (`os`) from the function name (`output`).
- The parentheses `()` contain the **arguments**—the values we're passing to the function. In this case, there's one argument: the string `"Hello, World!"`.
- The quotes `""` around `Hello, World!` indicate that it's a string—a piece of text.

**Step 3: Run the program.**
There are two ways to run your program:

**Option A: From the terminal.**
Open a terminal in VS Code by clicking Terminal > New Terminal. Then type:

```
apex hello.apex
```

You should see:

```
Hello, World!
```

Congratulations—you've just run your first Apex program!

**Option B: From VS Code.**
If you installed the Apex extension, you can run the program by pressing `F5` or by opening the Command Palette (Ctrl+Shift+P on Windows/Linux, Cmd+Shift+P on macOS), typing "Apex: Run Current File," and pressing Enter.

**Step 4: Experiment.**
Now that you have a working program, try changing it. Here are some ideas:

- Change the message to say something else: `os.output("Apex is awesome!")`
- Print multiple lines:
  ```apex
  import os
  os.output("Hello, World!")
  os.output("This is my first program.")
  os.output("I'm learning Apex!")
  ```

The important thing is that you've written and run your first program. You've taken the first step on a journey that will change how you think about computers and problem-solving.

Every expert programmer started exactly where you are now. The only difference between you and them is practice. So keep experimenting, keep asking questions, and don't be afraid to make mistakes. Mistakes are how we learn.

Welcome to Apex.

## Variables & Data Types
Every program you will ever write is, at its core, about doing things with information. That information might be a username, a price, a list of high scores, or whether a button has been clicked. But before your program can do anything useful, it needs a way to hold onto that information and know what kind of information it is. That's where variables and data types come in.

### What Is a Variable?
Think of a variable as a labeled box. You take a box, slap a label on it like `player_score`, and put a value inside it — say, the number `42`. Later, when you want to know the player's score, you don't need to remember the number itself. You just look at the box labeled `player_score` and see what's inside.

In Apex, creating a variable and putting a value in it looks like this:

```apex
player_score = 42
```

The single equals sign `=` means "put this value into this box." It's not saying that the two sides are equal like in math class — it's an instruction. It says: take the value on the right and store it in the variable named on the left.

A variable has three parts:

1. **Name** — the label on the box. You choose this. Good names describe what's inside: `player_score`.
2. **Value** — the actual data stored inside: `42`.
3. **Type** — what kind of data it is: `number`.

In Apex, you don't have to declare what type a variable will hold ahead of time. You don't write anything like "this box will only ever contain whole numbers." You just create the box, put something in it, and Apex figures out the type automatically. If you want to empty the box and put something completely different inside — a string where a number used to be — you can do that too.

### What Are Data Types?
A data type is simply a category of information. It tells your program what it can and can't do with a particular value. This matters because different kinds of data behave differently.

Here's a simple analogy: you wouldn't try to bite into a ceramic plate, and you wouldn't try to bake cookies on a paper napkin. Both are "things in your kitchen," but they're different *types* of things, and what you can do with them depends on their type.

The same is true in programming. The number `25` and the string `"25"` might look similar at a glance, but they are fundamentally different:

- The **number** `25` can be added to another number: `25 + 5` gives you `30`. That makes sense.
- The **string** `"25"` is not a quantity — it's text that happens to contain the characters `2` and `5`. Trying to add `"25" + 5` is like trying to add the word "twenty-five" to the number five. It doesn't compute.

Apex cares about data types because it needs to know what operations are allowed. When you write `price * quantity`, Apex knows that multiplication only makes sense with numbers. This is the whole point of types: they prevent you from accidentally doing nonsense, like trying to divide a table by a boolean.

### The Five Data Types in Apex
Apex keeps things refreshingly simple. There are only five data types you need to know about:

| Type | What It Holds | Example |
|------|---------------|---------|
| `none` | Nothing — the intentional absence of a value | `x = none` |
| `number` | Whole numbers and decimals | `x = 10`, `x = 3.14` |
| `string` | Text — any sequence of characters | `x = "hello"` |
| `boolean` | One of two values: `true` or `false` | `x = true` |
| `table` | A container that holds multiple values | `x = [1, 2, 3]` |

That's it. This simplicity is deliberate. Apex wants you to spend your time solving real problems, not wrestling with type declarations. In the subsequent parts of this section, we will delve deeper into data types.

### Why Types Matter Even When Apex Handles Them
You might be wondering: if Apex figures out types automatically, why do I need to learn about them at all? Fair question.

The answer is that Apex may not require you to declare types, but you still need to understand what kind of data your variables hold. Here's why.

First, certain operations only work with certain types. Arithmetic like `+`, `-`, `*`, `/`, `%` only works with numbers. You cannot multiply a string by a number. You cannot add a boolean to a table. If you write code that tries to do this, Apex will stop and tell you there's a problem. Understanding types helps you predict when this will happen before it does.

Second, comparisons behave differently depending on type. The number `5` and the string `"5"` are not equal in Apex. They look the same to a human, but Apex sees a number and a string — different boxes, different contents, not the same thing. If you compare them expecting `true`, you'll get `false` and wonder why.

Third, even though a variable's type can change, that doesn't mean it's a good idea to change it carelessly. A variable that holds a number on line 10, a string on line 25, and a table on line 40 is a recipe for confusion. You'll forget what it was supposed to be, and your code will become a puzzle for anyone reading it — including future you. Good programmers use the flexibility of dynamic typing with discipline: a variable's type *can* change, but it usually shouldn't.

### Putting It Together
Here's the mental model to carry with you:

- **Variables** are labeled boxes that hold information.
- **Data types** are categories that tell you what kind of information is in a box and what you can do with it.
- Apex figures out types automatically, but you still need to know what you're working with.
- The five types are `none`, `number`, `string`, `boolean`, and `table`.

In the following sections, we'll explore each data type in detail — how to create values of that type, what operations work with it, and the common pitfalls to avoid. By the end, you'll have an intuitive feel for which type to use in any situation.

## Numbers
Numbers are the foundation of computation. Counting items, calculating prices, tracking scores, measuring distances, timing events — if it involves quantity, it involves numbers. In Apex, working with numbers is designed to feel natural and frictionless.

### No Distinctions, No Friction
In many programming languages, numbers come in multiple flavors: integers, floats, doubles, longs, unsigned integers, and more. Each has different rules, different limits, and different gotchas. This is a source of endless confusion for beginners.

Apex sweeps all of that away. A number is a number. That's it.

```apex
apples = 4
temperature = -12
big_number = 1000000
price = 3.99
tiny = 0.00001
```

Notice that there's no special syntax for different kinds of numbers. You don't write `4` differently from `3.99`. You don't declare "this is a whole number" versus "this is a decimal." Apex figures out the details behind the scenes and lets you focus on your actual problem.

### Whole Numbers
Whole numbers — numbers without a decimal point — are written exactly as you'd expect:

```apex
year = 2024
count = 0
negative = -50
big_number = 1000000
```

Use whole numbers when you're counting things that can't be split into pieces: the number of users, the number of items in a cart, the number of times a loop has run.

### Decimal Numbers
For numbers with fractional parts, use a decimal point:

```apex
weight = 71.5
height = 1.83
tax_rate = 0.07
balance = -15.25
```

Important: Apex uses a dot `.` for decimals, not a comma. The comma has a different job in Apex — it separates items in tables and arguments in function calls. If you write `3,14` expecting a decimal number, Apex will not understand what you mean.

Use decimals when precision matters: money, measurements, percentages, scientific values.

### Positive and Negative
Numbers can be positive or negative. Negative numbers are written with a minus sign directly before the number:

```apex
temperature = -5
balance = -100.50
```

Positive numbers can optionally have a plus sign, but nobody does this — it's just `5`, not `+5`.

## Strings
Numbers are great for counting and calculating, but most of the information humans deal with every day isn't numeric. Your name, a street address, the title of a song, an email message, the label on a button — these are all sequences of characters. In programming, we call this kind of data a **string**.

Think of a string as a chain of characters linked together. The word `"hello"` is a string made of five characters: `h`, `e`, `l`, `l`, `o`. A string can be a single character, a thousand characters, or even zero characters — an empty string with nothing inside.

### Creating Strings
In Apex, you create a string by wrapping text in quotes. You can use either double quotes `"..."` or single quotes `'...'`. Both work exactly the same way:

```apex
first_name = "Alice"
last_name = 'Smith'
empty_string = ""
single_character = "A"
```

The quotes are not part of the string itself — they're just markers that tell Apex "everything between these is text." The string `"Alice"` contains five characters: `A`, `l`, `i`, `c`, `e`. The quotes are only there for Apex to know where the text begins and ends.

Why two kinds of quotes? Because sometimes your text contains a quote character. If you want to write a string with an apostrophe inside, use double quotes on the outside:

```apex
message = "It's a beautiful day"
```

If you want to write a string with a double quote inside, use single quotes on the outside:

```apex
quote = 'He said "hello" to me'
```

This way you rarely need to worry about quotes colliding. Choose whichever outer quotes make your text easiest to write.

### Strings Are Not Numbers
This is worth repeating because it's one of the most common sources of confusion for beginners. The string `"42"` and the number `42` are completely different things in Apex.

```apex
age_as_string = "42"
age_as_number = 42
```

They might look similar to your eyes, but Apex treats them very differently. The number `42` is a quantity — you can add it, subtract it, multiply it. The string `"42"` is text — it happens to contain the characters `4` and `2`, but you can't do math with it any more than you can do math with the word `"apple"`.

This distinction matters when you start combining values. Later, when you learn about arithmetic operators, you'll see that trying to add a number to a string makes no sense to Apex, and it will stop and tell you so. For now, just remember: if it's in quotes, it's text, not a quantity.

### Escape Sequences
Sometimes you need to include special characters in a string — characters that would normally break the string or that you can't type directly. Apex gives you a mechanism called **escape sequences** to handle these situations.

An escape sequence starts with a backslash `\` followed by another character. The backslash tells Apex: "The next character is special — don't treat it the way you normally would."

Let's look at the escape sequences Apex supports and when you'd use each one.

#### Quotes Inside Strings
Suppose you want to create a string that contains a double quote, and you also want to use double quotes on the outside. The naive approach fails:

```apex
sentence = "He said "hello" to me"
```

Apex reads this as: the string starts with the first `"`, then `He said `, then the second `"` ends the string. After that, `hello` is floating in space — not part of any string — and Apex gets confused. The problem is that the inner quotes are indistinguishable from the outer quotes.

The solution is to escape the inner quotes with a backslash:

```apex
sentence = "He said \"hello\" to me"
```

When Apex sees `\"`, it understands: "This quote is meant to be printed as part of the text, not to end the string." The same works for single quotes:

```apex
sentence = 'It\'s a wonderful day'
```

Here, the apostrophe in `It's` would normally end a single-quoted string. The backslash before it prevents that.

#### Line Breaks with `\n`
Sometimes you want a line break inside a string, but you're writing a short string and want to keep everything on one line of code. For that, you use the escape sequence `\n`, where `n` stands for "newline."

```apex
message = "First line\nSecond line\nThird line"
```

When Apex encounters `\n`, it doesn't print those two characters. It inserts an actual line break into the text. If you were to display this string, you'd see:

```text
First line
Second line
Third line
```

The `\n` is invisible — it's a command to move to the next line, not something that appears in the output.

For short strings, `\n` keeps everything compact. But if you're writing longer text with many line breaks, there's a more readable option coming up in the **Multiline Strings** section — where you can simply press Enter in your code, and Apex will understand it as part of the string.

#### Tabs with `\t`
The escape sequence `\t` inserts a tab character. Tabs are useful for aligning text into columns, especially when you want to display data in a table-like format without building an actual table.

```apex
header = "Name\tAge\tCity"
```

Displaying this string gives you evenly spaced columns:

```text
Name    Age    City
```

The tab character pushes the next piece of text to the next tab stop, creating consistent spacing regardless of how long the preceding text is.

#### Escaping the Backslash Itself
Here's a puzzle: what if you actually want a backslash to appear in your string? File paths on some systems use backslashes, and you might need to include them in text.

The problem is that a single backslash is always interpreted as the start of an escape sequence. If you write:

```apex
path = "C:\Users\Alice"
```

Apex sees `\U` and thinks "this must be some kind of escape sequence" — and then gets confused because `\U` isn't a supported escape. The solution is to escape the backslash itself by doubling it:

```apex
path = "C:\\Users\\Alice"
```

Each `\\` tells Apex: "I want an actual backslash in my text, not the start of an escape sequence." The string itself contains single backslashes.

#### Other Escape Sequences
Apex also supports a few additional escape sequences for less common situations:

| Escape | Meaning                                         |
|--------|-------------------------------------------------|
| `\n`   | New line                                        |
| `\t`   | Tab                                             |
| `\r`   | Carriage return (moves cursor to start of line) |
| `\"`   | Double quote                                    |
| `\'`   | Single quote                                    |
| `\\`   | Backslash                                       |
| `\{`   | Literal curly brace                             |
| `\}`   | Literal curly brace                             |
| `\0`   | Character code in octal notation                |

The octal notation is a more advanced feature — it lets you insert any character by its numeric code. Most beginners won't need it, but it's good to know it exists.

The escape sequences for curly braces — `\{` and `\}` — are special. Curly braces have a special meaning in Apex strings, which we'll cover in just a moment. If you need literal curly braces in your text, those escapes are how you get them.

### Multiline Strings
Escape sequences like `\n` work, but if you're writing a long piece of text — an email body, a poem, a formatted message — sprinkling `\n` everywhere gets ugly fast. The text becomes hard to read and even harder to edit.

Apex gives you a cleaner way: **multiline strings**. You simply press Enter inside the string and keep typing. The line breaks in your code become actual line breaks in the text.

```apex
email = "
    Hello,

    Thank you for your purchase.

    Your order has been shipped.

    Best regards,
    The Store Team
"
```

Apex captures the text exactly as you wrote it — line breaks, indentation, everything. When displayed, this string looks exactly like what's between the quotes. No `\n` noise, no escaping headaches, just natural text.

This is especially useful for any kind of formatted output: letters, reports, multi-line messages, or code templates.

### String Interpolation
Often you don't just want a fixed piece of text — you want to embed the value of a variable inside a larger message. For example, you might want to say "Hello, Alice" where `Alice` is stored in a variable called `name`.

Apex gives you a clean, readable tool for this: **string interpolation**.

To embed a variable's value inside a string, write the variable name inside curly braces `{}`:

```apex
name = "Alice"
greeting = "Hello, {name}"
```

When Apex sees `{name}` inside the string, it doesn't print those six characters literally. It looks up the variable `name`, takes its value, and inserts that value into the string. The result is:

```text
Hello, Alice
```

You can interpolate any variable you've created:

```apex
name = "Alice"
age = 30
city = "Dubai"
message = "{name} is {age} years old and lives in {city}"
```

Apex automatically converts non-string values to their text representation. The variable `age` holds the number `30`, but inside the interpolation braces Apex turns it into the string `"30"` so it can be embedded in the message.

You can also put simple expressions inside the braces, not just variable names. If you want to do a quick calculation and include the result in your text, you can:

```apex
count = 5
message = "Total items: {count * 2}"
```

Apex evaluates the expression `count * 2`, gets `10`, converts it to `"10"`, and embeds it in the string. This is handy for quick inline calculations without needing to create a separate variable first.

### Curly Braces in Strings
Since curly braces have a special meaning in Apex strings — they trigger interpolation — you might wonder what happens if you actually want curly braces in your text. Perhaps you're writing a template that should be filled in later, or you're documenting code snippets.

If you write:

```apex
template = "Hello {user}, your balance is {amount}"
```

Apex will try to find variables named `user` and `amount` and insert their values. If those variables don't exist, you'll get an error.

But if you *want* the literal text `{user}` to appear in your string — curly braces and all — you can escape the braces with a backslash:

```apex
template = "Hello \{user\}, your balance is \{amount\}"
```

The `\{` tells Apex: "This curly brace is plain text, not the start of an interpolation." The resulting string contains the literal characters `{user}` and `{amount}` without any substitution.

### Putting It Together
Strings are how your program talks to people. Every message you display, every name you store, every piece of text you manipulate is a string. Here's what to remember:

- Strings are created with quotes: `"double"` or `'single'`.
- Strings are text, not numbers — `"42"` and `42` are different things.
- Escape sequences let you include special characters: `\n` for new lines, `\t` for tabs, `\"` and `\'` for quotes, `\\` for backslashes.
- Multiline strings let you write long text naturally, with line breaks in your code becoming line breaks in the output.
- String interpolation with `{variable}` embeds values directly into text.
- To get literal curly braces in a string, escape them: `\{` and `\}`.

Strings are one of the two data types you'll use more than any other — the other being numbers. In the next section, we'll explore the final data type: tables, which let you group multiple values together.

## Booleans
So far you've met two kinds of data: numbers for quantities and strings for text. Now we meet a new data type — one that's small in size but enormous in importance. It's called a **boolean**, and it can hold exactly one of two values: `true` or `false`.

That's it. No numbers, no text, no shades of gray. A boolean is a switch that's either on or off. It's the answer to a yes-or-no question.

### Why Booleans Exist
Think about how many things in life come down to a simple yes or no:

- Is the user logged in?
- Is the cart empty?
- Did the file save successfully?
- Is this person over 18?

These aren't questions with numeric answers. The answer isn't `0` or `"maybe"`. The answer is either yes or no, and that's exactly what a boolean captures.

Programs make decisions constantly, and every decision starts with a boolean. "If the user is logged in, show their dashboard." "If the cart is not empty, allow checkout." The boolean is the signal that tells your program which path to take.

### Creating Booleans
The simplest way to get a boolean is to write it directly:

```apex
is_logged_in = true
has_permission = false
is_active = true
is_deleted = false
```

Direct assignment is straightforward, but booleans become truly useful when they're *produced* by something. The most common source of boolean values is comparison — asking Apex to check whether something is the case.

You do this with comparison symbols:

```apex
age = 25
is_adult = age > 18
```

Here's what happens on that second line. The expression `age > 18` is a question: "Is the value of `age` greater than 18?" Apex checks, determines the answer is yes, and produces the boolean value `true`. That `true` is then stored in the variable `is_adult`. The same works for other kinds of comparisons:, Apex answers with `true` or `false`, and that answer gets stored in a variable. We'll explore all the comparison symbols in detail in the Operators section. For now, what matters is the core idea: comparisons create booleans.

### Naming Boolean Variables
Because booleans represent yes-or-no answers, their names should sound like questions or statements that can be true or false. A common convention is to start the name with `is_`, `has_`, `can_`, or `should_`:

```apex
is_logged_in = true
has_access = false
can_edit = true
should_save = false
```

These names read naturally: "is logged in" — yes or no? "has access" — yes or no? When someone reads your code, they immediately understand that these variables hold booleans and what question they answer.

Avoid names that are vague about their meaning:

```apex
status = true       // what does this mean?
flag = false        // what kind of flag?
enabled = true      // enabled what?
```

Better names describe exactly what's true or false:

```apex
is_online = true
has_errors = false
notifications_enabled = true
```

### Booleans Are Not Strings
It's worth emphasizing one common pitfall. The string `"true"` and the boolean `true` are different things:

```apex
logged_in = true          // boolean
logged_in = "true"        // string — completely different type
```

The first one is a boolean that answers "yes" to the question "is the user logged in?" The second is a piece of text that happens to spell out the word "true." Apex treats them differently because they are different. You can't do the same things with them, and comparing one to the other will give you `false`. Keep them separate in your mind. If it's in quotes, it's text. If it's bare `true` or `false`, it's a boolean.

### Booleans as Data
Let's end with a quick example that shows how booleans fit alongside the other data types you've learned:

```apex
name = "Alice"              // string
age = 30                    // number
is_active = true            // boolean
has_subscription = false    // boolean
```

Here we have four variables, three different types. The strings and numbers describe Alice. The booleans answer questions about her: Is she active? Yes. Does she have a subscription? No.

This is how real programs work. You'll often have a mix of types describing one thing — and the booleans among them capture the yes-or-no aspects.

## Tables
You've now met numbers, strings, and booleans. Each of these holds a single value — one number, one piece of text, one true-or-false answer. But real programs rarely deal with just one thing at a time. A shopping cart has many items. A user profile has a name, an email, an age, and a subscription status. A high-score list has dozens of entries.

You need a way to group multiple values together, and that's exactly what a **table** is for.

Think of a table as a container — a box that can hold many other boxes inside it. Unlike a variable that holds one value, a table can hold ten values, a hundred values, or even a thousand values, all organized so you can find each one when you need it.

### Creating an Empty Table
The simplest table is one with nothing in it. You create it with a pair of square brackets:

```apex
empty = []
```

This creates an empty container. It exists, but it holds nothing. It's like an empty backpack — ready to be filled with items later.

### Creating a Table with Values
To create a table that already contains values, list them inside the square brackets, separated by commas:

```apex
fruits = ["apple", "banana", "cherry"]
numbers = [10, 20, 30, 40, 50]
mixed = [42, "hello", true]
```

Each of these is a table. The first holds three strings. The second holds five numbers. The third holds a mix — a number, a string, and a boolean. Tables don't care what types they contain. You can put any combination of values inside.

### Ordered Lists
When you create a table by simply listing values — like `["apple", "banana", "cherry"]` — you're creating an **ordered list**. Each value has a position, and those positions are numbered starting from 1. This is a crucial detail, because many programming languages start counting from 0, but Apex follows the more natural human convention. The first item is at position 1, the second at position 2, and so on.

```apex
colors = ["red", "green", "blue"]
```

In this table:

- Position 1 holds `"red"`
- Position 2 holds `"green"`
- Position 3 holds `"blue"`

To access a value in a table, you write the table's name, followed by square brackets containing the position:

```apex
colors = ["red", "green", "blue"]
first_color = colors[1]       // "red"
second_color = colors[2]      // "green"
third_color = colors[3]       // "blue"
```

The expression `colors[1]` means: "Look inside the table called `colors`, and give me the value at position 1." You can use this anywhere you'd use a regular value — assign it to a variable, display it, or do anything else.

### Adding and Changing Items
Once a table exists, you can add new values to it or change existing ones. This is done with the same square-bracket syntax, combined with the assignment operator `=`:

```apex
fruits = ["apple", "banana"]
fruits[3] = "cherry"      // adds "cherry" at position 3
```

Now the table contains three items. You can also change an existing value:

```apex
fruits = ["apple", "banana", "cherry"]
fruits[2] = "blueberry"   // replaces "banana" with "blueberry"
```

The position numbers don't have to be in order, though it's usually cleaner if they are. What matters is that each position gives you a way to store and retrieve a value.

### Key-Value Pairs
Ordered lists are useful when your data is naturally a sequence — the first thing, the second thing, the third thing. But often your data isn't sequential. Consider a user profile:

- The name is "Alice"
- The age is 30
- The email is "alice@example.com"
- The account is active

There's no meaningful "first" or "second" here. You don't think of Alice's age as "position 2 of her profile." You think of it as "the value associated with the word 'age'."

For this kind of data, tables support **key-value pairs**. A key is a label — a name you choose — and it's connected to its value with an equals sign:

```apex
user = [
    "name" = "Alice",
    "age" = 30,
    "active" = true
]
```

Here, the table has three entries, but they're not numbered 1, 2, 3. They're labeled with keys:

- The key `"name"` is associated with the value `"Alice"`
- The key `"age"` is associated with the value `30`
- The key `"active"` is associated with the value `true`

To access these values, you use the key inside square brackets:

```apex
user = [
    "name" = "Alice",
    "age" = 30,
    "active" = true
]

user_name = user["name"]       // "Alice"
user_age = user["age"]         // 30
user_active = user["active"]   // true
```

The expression `user["name"]` means: "Look inside the table called `user`, and give me the value associated with the key `"name"`."

Keys are always strings. In the examples above, `"name"`, `"age"`, and `"active"` are string keys. You cannot use numbers as keys because numbers are already used for positions in ordered lists.

### Adding and Changing Key-Value Pairs
Just like with ordered lists, you can add new key-value pairs or change existing ones after the table is created:

```apex
user = ["name" = "Alice"]

user["age"] = 30            // adds a new key "age"
user["city"] = "Dubai"      // adds a new key "city"
user["name"] = "Alicia"     // changes the value under "name"
```

After these lines, the table has three keys: `"name"` (now `"Alicia"`), `"age"` (with value `30`), and `"city"` (with value `"Dubai"`).

### Accessing a Key That Doesn't Exist
What happens if you try to access a key that isn't in the table?

```apex
user = ["name" = "Alice"]
email = user["email"]
```

There is no key called `"email"` in this table. So what value does `email` get?

The answer: it gets `none`.

`none` is a special value in Apex that means "there is nothing here." It's the absence of any value at all. We'll explore `none` in detail in the next section, but for now, know this: when you ask a table for a key that doesn't exist, you get back `none` instead of an error.

This is actually very useful. It gives you a way to check whether a key exists. We'll learn how to check for this explicitly when we cover comparison operators and if statements.

### Mixed Tables
Here's a powerful feature of Apex tables: you can combine ordered lists and key-value pairs in the same table. Ordered items come first, then key-value pairs:

```apex
person = ["Alice", "Manager", "department" = "Engineering", "years" = 5]
```

This table contains both kinds of entries. The first two values — `"Alice"` and `"Manager"` — are ordered items at positions 1 and 2. The last two entries are key-value pairs.

You access each kind the same way you would in a pure list or pure key-value table:

```apex
name = person[1]                 // "Alice" — by position
role = person[2]                 // "Manager" — by position
dept = person["department"]      // "Engineering" — by key
experience = person["years"]     // 5 — by key
```

Mixed tables let you represent data that has both a natural ordering and labeled attributes. For example, a row from a spreadsheet might have positional values plus metadata about what those values mean.

### Tables Inside Tables
A table can hold any type of value — including other tables. This lets you build complex, nested structures that represent real-world data.

Here's an example: a company with a name, a list of employees, and an address:

```apex
company = [
    "name" = "Apex Corp",
    "employees" = ["Alice", "Bob", "Charlie"],
    "address" = [
        "street" = "1 Main Street",
        "city" = "Dubai",
        "country" = "UAE"
    ]
]
```

Let's unpack this. The outer table is called `company`. It has three keys:

- `"name"` — a string: `"Apex Corp"`
- `"employees"` — a table: `["Alice", "Bob", "Charlie"]`
- `"address"` — a table: another key-value table inside

To access the inner values, you chain square brackets:

```apex
company_name = company["name"]                       // "Apex Corp"
first_employee = company["employees"][1]             // "Alice"
city = company["address"]["city"]                    // "Dubai"
```

Let's trace through `company["employees"][1]`:

1. `company["employees"]` goes into the outer table and pulls out the employees table: `["Alice", "Bob", "Charlie"]`
2. `[1]` then goes into that inner table and pulls out the value at position 1: `"Alice"`

Similarly, `company["address"]["city"]` first extracts the address table, then extracts the value under the `"city"` key.

You can nest as deeply as you need:

```apex
school = [
    "name" = "Central High",
    "classes" = [
        [
            "teacher" = "Mr. Smith",
            "students" = ["Alice", "Bob"]
        ],
        [
            "teacher" = "Ms. Jones",
            "students" = ["Charlie", "Diana"]
        ]
    ]
]

first_teacher = school["classes"][1]["teacher"]       // "Mr. Smith"
second_class_first_student = school["classes"][2]["students"][1]   // "Charlie"
```

Each level of square brackets digs one level deeper into the structure. It's like navigating a folder system: you open the outer folder, then the inner folder, then grab the file you want.

### A Quick Word on Positions vs. Keys
You might be wondering: what's the difference between `table[1]` and `table["key"]`?

- `table[1]` uses a **position** — a number that tells Apex which item you want, based on its order.
- `table["key"]` uses a **key** — a string label that tells Apex which value you want, based on its name.

The syntax looks similar, but they work differently. Positions are for ordered data, keys are for labeled data. A table can use both systems at once — which is what makes mixed tables possible.

## None
Every data type you've met so far represents something. Numbers represent quantities. Strings represent text. Booleans represent true or false. Tables represent collections of values. But sometimes you need to represent *nothing at all* — and for that, Apex has a special data type called `none`.

Think back to the labeled box analogy for variables. A variable is a box with a label, and you put a value inside it. But what if you have a box that's intentionally empty? The box exists, it has a label, but there's nothing inside. That's what `none` is: a deliberate empty space where a value could be, but isn't.

This is different from a box that was never created. A variable that doesn't exist is not the same as a variable that exists and holds `none`. The first is an error waiting to happen. The second is a valid state — the program is explicitly saying "there is no value here right now."

### Not an Empty String or Table
It's important to distinguish `none` from other values that might seem similar at first glance:

```apex
empty_number = 0
empty_string = ""
empty_table = []
empty_value = none
```

Each of these is different:

- `0` is a number. It's a real value — you can add it, subtract it, use it in calculations. It answers the question "how many?" with "zero."
- `""` is a string. It's a piece of text with zero characters in it. It's still text — you can check its length, combine it with other strings, and so on.
- `[]` is a table. It's a container with nothing inside. The container exists; it's just empty.
- `none` is none of these. It's not a number, not a string, not a table, not a boolean. It's the complete absence of any value.

Think of it this way: an empty glass isn't the same as no glass at all. `0`, `""`, and `[]` are empty glasses — they have a type and a structure, but no contents. `none` is no glass at all.

### The Role of `none`
If you worked through the previous section on tables, you've already encountered `none` in practice. Recall what happens when you try to access a key that doesn't exist in a table:

```apex
user = ["name" = "Alice"]
email = user["email"]
```

The table `user` has only one key: `"name"`. There is no `"email"` key. When you ask for it, Apex can't give you a value because there isn't one. So it gives you `none` instead.

This isn't an error. Apex doesn't stop and complain. It simply returns `none`, and your program continues. The variable `email` now holds `none`, which tells you: "There was nothing under that key."

This is a common pattern. When you're not sure whether a key exists, you access it and check whether you got `none` back. If you did, the key wasn't there. If you got an actual value, it was.

You might wonder why a language needs a special value for "nothing." Why not just not create the variable at all, or leave it undefined?

The reason is that programs need to talk about absence explicitly. Sometimes a piece of code looks for something and doesn't find it — like searching for a user that doesn't exist. The code needs a way to say "I looked, and there was nothing there" without crashing your program or giving a misleading answer. `none` is that answer. You'll see this pattern constantly when we get to functions later in the book.

A common use of `none` is to set up a variable before you know what should go in it:

```apex
selected_user = none
```

Later in your program, when someone actually selects a user, you'll replace the `none` with a real value:

```apex
selected_user = "Alice"
```

This pattern — starting with `none` and filling in later — is very common. It lets you create all your variables up front, even if you don't know their final values yet.

## Constant
Throughout this section, you've been creating variables and changing their values freely. You assign a value, then assign a new one, and Apex happily updates the variable. This flexibility is useful, but sometimes you want the opposite: a value that should never change after it's been set. That's what `constant` gives you.

### The Problem Constants Solve
Think about the number of hours in a day. It's 24. Always has been, always will be. If you store that value in a variable:

```apex
hours_in_day = 24
```

What happens if, later in your program, you accidentally write:

```apex
hours_in_day = 25
```

Apex won't complain. It will dutifully replace 24 with 25, and now your program thinks there are 25 hours in a day. Every calculation that uses `hours_in_day` will be wrong, and you might not notice until something breaks badly.

The problem here isn't that changing a variable is bad — it's that some values shouldn't be changeable. They're facts about your program that should stay fixed forever. `constant` lets you tell Apex: "This value is locked. Nobody is allowed to change it."

### What Constant Does
`constant` is not a new data type. It's a modifier — a word you put before a variable name to change how that variable behaves. The variable still holds a number, string, boolean, table, or `none`. The only difference is that once you assign a value, you can't assign it again.

Here's how you create a constant:

```apex
constant HOURS_IN_DAY = 24
```

The word `constant` comes first, followed by the variable name, followed by the assignment. It looks almost exactly like a regular variable declaration, with one extra word at the front.

Now, if you try to change it:

```apex
constant HOURS_IN_DAY = 24
HOURS_IN_DAY = 25
```

Apex will stop and report an error. It will not let the assignment go through. The variable `HOURS_IN_DAY` remains locked at 24.

### Constants and Data Types
A constant can hold any of the five data types. The `constant` modifier doesn't care what kind of value you're storing — it only prevents reassignment:

```apex
constant APP_NAME = "Apex"                       // constant string
constant MAX_RETRIES = 3                         // constant number
constant IS_DEBUG = false                        // constant boolean
constant DEFAULT_SETTINGS = ["theme" = "light"]  // constant table
constant NO_VALUE = none                         // constant none
```

Each of these variables holds a value that cannot be changed. The types are exactly the same as they would be for regular variables — only the mutability differs.

### A Note on Naming
You may have noticed that the constant examples use `ALL_CAPS` names like `HOURS_IN_DAY` and `MAX_RETRIES`. This is a common convention — a style rule, not a language requirement.

The idea is simple: when you see a name in all capital letters, you immediately know "this is a constant — it doesn't change." It's a visual signal that helps you and anyone reading your code understand what's fixed and what's flexible.

Apex doesn't require this. You could name a constant `hours_in_day` and it would work exactly the same. But using `ALL_CAPS` for constants and regular lowercase for changeable variables is a good habit that makes your code clearer.

## Built-in Functions
You've now met all five data types and learned how to create variables that hold them. But knowing how to store data is only half the picture. You also need tools to work with that data — to convert it, inspect it, and understand it. Apex provides a small set of **built-in functions** for exactly this purpose.

Before we dive in, let's clarify what a function is. You'll learn to create your own functions in a later section, but for now, think of a function as a named tool that takes some input, does something with it, and gives back a result. You use a function by writing its name, followed by parentheses. Inside the parentheses, you put the input — the value you want the function to work on. The function then returns a result.

```apex
number("42")
```

Here, `number` is the function's name. The value inside the parentheses — `"42"` — is the input, called an **argument**. The function takes that argument, does its work, and gives back a result. In this case, the result is the number `42`.

### Three Essential Built-ins
Apex provides three built-in functions that you'll use constantly, especially as a beginner:

| Function    | What It Does                                    |
|-------------|-------------------------------------------------|
| `type(x)`   | Tells you what type a value is                  |
| `number(x)` | Tries to convert a value to a number            |
| `string(x)` | Converts any value to its string representation |

Each takes one argument — the value inside the parentheses — and returns something useful. Let's explore each in detail.

### type(): Checking What Something Is
The `type()` function answers a simple question: "What kind of value is this?" You give it any value, and it returns a string telling you the type.

```apex
type(42)          // "number"
type("hello")     // "string"
type(true)        // "boolean"
type(none)        // "none"
type([1, 2, 3])   // "table"
```

The result is always one of five strings: `"number"`, `"string"`, `"boolean"`, `"none"`, or `"table"`. Notice that these are strings — they're text, not the values themselves. When you see `"number"` in quotes, that's a string saying "this value is a number type."

You can use `type()` with variables too:

```apex
name = "Alice"
age = 30
is_active = true

type(name)       // "string"
type(age)        // "number"
type(is_active)  // "boolean"
```

When would you actually use this? Imagine you're working with a variable whose value came from somewhere else — user input, a table lookup, a function you didn't write. You're not sure what type it is. `type()` gives you certainty.

### number(): Converting to a Number
The `number()` function tries to take whatever value you give it and turn it into a number. It works in two cases: when the value is already a number, and when the value is a string that contains numeric text.

**Converting strings to numbers:**
The most common use is turning a string like `"42"` into the number `42`. This matters because strings and numbers are different types, and sometimes you receive data as text that you need to do math with.

```apex
number("42")    // 42
number("3.14")  // 3.14
number("-7")    // -7
```

In each case, the input is a string containing numeric characters, and the output is an actual number you can use in calculations.

**Numbers stay numbers:**
If you give `number()` a value that's already a number, you get that number back unchanged:

```apex
number(10)    // 10
number(3.14)  // 3.14
```

**When conversion fails:**
What happens if you give `number()` something that can't sensibly be turned into a number? Like a string of text, or a boolean, or a table?

```apex
number("hello")  // none
number(true)     // none
number(false)    // none
number([])       // none
```

The answer: you get `none` back. This makes sense when you think about it. The string `"hello"` doesn't contain any numeric value. The boolean `true` isn't a quantity. A table isn't a number. There's no way to convert these to numbers, so `number()` returns `none` to say "I couldn't do it."

This is a perfect example of `none` being useful. The function always returns *something*, but when conversion is impossible, it returns `none` instead of a number. Your program can then check whether the result is `none` to know whether the conversion succeeded.

**A practical example:**
User input is almost always text. Even if someone types `42` at a prompt, your program receives the string `"42"`, not the number `42`. If you want to do math with that input, you must convert it:

```apex
user_input = "25"         // imagine this came from keyboard input
age = number(user_input)  // now age is the number 25
next_year = age + 1       // 26 — math works because age is a number
```

Without the conversion, `age` would be the string `"25"`, and trying to add `1` to it wouldn't work.

### string(): Converting to a String
The `string()` function is the opposite of `number()`. It takes any value and turns it into its string representation — the text form of that value.

**Numbers to strings:**
```apex
string(42)    // "42"
string(3.14)  // "3.14"
string(-7)    // "-7"
```

The number `42` becomes the string `"42"`. They look the same to human eyes, but now it's text. You can't do math with it anymore, but you can do text things with it — like embed it in a larger string.

**Booleans to strings:**
```apex
string(true)   // "true"
string(false)  // "false"
```

**none to string:**
```apex
string(none)  // "none"
```

**Tables to string:**
```apex
string([])  // "[]"
```

The table conversion produces a text representation of the table's contents.

### Built-ins Are Functions Like Any Other
These three functions — `type`, `number`, and `string` — are exactly the same kind of thing as the functions you'll learn to create later in this book. They take arguments, they return results, and they can be used anywhere a value is expected. The only difference is that Apex provides them automatically. You don't need to create them or import anything — they're just there, ready to use from the moment you start writing code.

In fact, you've already used another built-in function without realizing it:

```apex
os.output("Hello")
```

The `output` function from the `os` library is also a function — it takes a string as an argument and displays it on screen. The same pattern applies: function name, parentheses, argument inside. The difference is that `output` comes from the `os` library, while `type`, `number`, and `string` are available everywhere without any import.

## Arithmetic Operators
You've learned how to store values in variables. Now it's time to do something with those values. Operators are the tools that let you work with data — combining values, performing calculations, and asking questions about them. We'll start with the most familiar kind: arithmetic operators.

### What Is an Operator?
An operator is a symbol that tells Apex to perform a specific action on one or more values. You already know operators from everyday math: the plus sign `+` means "add these together," the minus sign `-` means "subtract this from that." Apex uses these same symbols, plus a few more.

In programming, the values that an operator works on are called **operands**. In the expression `5 + 3`, the operands are `5` and `3`, and the operator is `+`. The whole expression evaluates to a result: `8`.

You can use operators directly in your code:

```apex
result = 5 + 3
```

Here, Apex evaluates `5 + 3`, gets `8`, and stores that result in the variable `result`.

### The Five Arithmetic Operators
Apex provides five arithmetic operators:

| Operator | Name               | Example  | Result |
|----------|--------------------|----------|--------|
| `+`      | Addition           | `5 + 3`  | `8`    |
| `-`      | Subtraction        | `10 - 4` | `6`    |
| `*`      | Multiplication     | `7 * 6`  | `42`   |
| `/`      | Division           | `15 / 4` | `3.75` |
| `%`      | Modulo (remainder) | `15 % 4` | `3`    |

Each of these works with numbers. Let's explore each one.

### Addition
Addition uses the plus sign `+`. It adds two numbers together:

```apex
sum = 5 + 3      // 8
total = 10 + 25  // 35
```

Addition also works with variables:

```apex
price = 25
tax = 3.75
total = price + tax  // 28.75
```

When you write `price + tax`, Apex looks up the values stored in those variables and adds them together. The result is a new number, which gets stored in `total`.

### Subtraction
Subtraction uses the minus sign `-`. It subtracts the right operand from the left operand:

```apex
difference = 10 - 4      // 6
remaining = 100 - 30     // 70
```

With variables:

```apex
balance = 100
withdrawal = 30
remaining = balance - withdrawal    // 70
```

The order matters in subtraction. `10 - 4` gives `6`, but `4 - 10` gives `-6`. Apex always subtracts in the order you write: left side minus right side.

### Multiplication
Multiplication uses the asterisk `*`, not the letter `x` or the `×` symbol. On a keyboard, the asterisk is the multiplication sign:

```apex
product = 5 * 3          // 15
area = 10 * 20           // 200
```

With variables:

```apex
width = 5
height = 3
area = width * height    // 15
```

Multiplication is commutative — the order doesn't matter. `5 * 3` and `3 * 5` both give `15`. But it's still good practice to write expressions in a logical order.

### Division
Division uses the forward slash `/`. It divides the left operand by the right operand:

```apex
quotient = 15 / 3        // 5
half = 10 / 2            // 5
```

With variables:

```apex
total = 100
people = 4
share = total / people   // 25
```

**Division and decimals:**
Here's something important about division in Apex: it always gives you the exact result, including decimal parts. It doesn't round or truncate:

```apex
7 / 2 = 3.5        // not 3 — Apex keeps the decimal
1 / 3 = 0.333333   // keeps as much precision as possible
```

This is different from some other programming languages where dividing two whole numbers gives you a whole number result with the decimal part thrown away. Apex doesn't do that. If the division has a remainder, you get a decimal answer.

**Division by zero:**
In many programming languages, dividing by zero causes an error and crashes your program. Apex takes a different approach. Instead of stopping everything, it follows the IEEE 754 standard for floating-point arithmetic. Under this standard, division by zero produces special values instead of errors.

Here's exactly why each result appears.

**You get `inf` when:**
A positive number is divided by positive zero. The dividend has a positive sign, the divisor has a positive sign. Signs match, result is positive, and the magnitude grows without bound:

```apex
result = 10 / 0    // inf
result = -10 / -0  // inf — both negative, signs cancel
```

**You get `-inf` when:**
The signs of the dividend and divisor don't match. One is positive, the other is negative:

```apex
result = -10 / 0  // -inf — negative divided by positive
result = 10 / -0  // -inf — positive divided by negative
```

**You get `nan` when:**
Zero is divided by zero. The mathematical answer doesn't exist — it's not infinity because there's no direction, it's not a number because nothing meaningful emerges:

```apex
result = 0 / 0    // nan
result = -0 / -0  // nan — both negative, signs cancel, still undefined
```

**You get `-nan` when:**
Zero is divided by zero, and the signs don't match. One zero is positive, the other is negative. The undefined result inherits the mismatched sign:

```apex
result = 0 / -0  // -nan — positive zero divided by negative zero
result = -0 / 0  // -nan — negative zero divided by positive zero
```

**Why signs matter:**
IEEE 754 tracks the sign of zero separately from its magnitude. Positive zero and negative zero are distinct values. When division produces infinity, the sign comes from combining the signs of the operands. When division produces NaN, the sign comes from whether those signs disagreed.

Apex doesn't crash on any of these. It produces the special value and keeps running. But `nan` and `-nan` are not numbers you can use in normal calculations. Any arithmetic involving them spreads the `nan` further. If you see `nan` in your output, somewhere earlier a calculation produced something that isn't a number.

### Modulo
Modulo is the one operator that might be new to you. Written as the percent sign `%`, it gives you the **remainder** after division.

Think back to elementary school division. When you divide 10 by 3, you get 3 with a remainder of 1. The modulo operator gives you just that remainder:

```apex
10 % 3 = 1  // 10 divided by 3 is 3 with remainder 1
15 % 4 = 3  // 15 divided by 4 is 3 with remainder 3
20 % 5 = 0  // 20 divided by 5 is 4 with remainder 0
```

When the division is exact — no remainder — modulo gives you `0`:

```apex
20 % 5 = 0
100 % 10 = 0
```

When the left number is smaller than the right number, modulo gives you the left number back:

```apex
3 % 10 = 3  // 3 divided by 10 is 0 with remainder 3
7 % 8 = 7   // 7 divided by 8 is 0 with remainder 7
```

**What is modulo used for?**
The most common use is checking whether a number is even or odd. Any number that divides evenly by 2 is even; any number that doesn't is odd:

```apex
8 % 2 = 0  // even — no remainder
9 % 2 = 1  // odd — remainder of 1
```

Another use: checking whether one number divides evenly into another:

```apex
15 % 5 = 0     // 15 is divisible by 5
15 % 4 = 3     // 15 is not divisible by 4
```

Modulo is also useful for "wrapping around" — like when you want a counter to go 0, 1, 2, 0, 1, 2 and never exceed 2:

```apex
counter = 5
wrapped = counter % 3    // 2 — because 5 divided by 3 has remainder 2
```

We'll see modulo used in practical ways later.

### Operator Precedence
When an expression contains multiple operators, Apex doesn't simply work left to right. It follows the same rules you learned in math class: multiplication and division happen before addition and subtraction.

```apex
result = 2 + 3 * 4
```

Here's what happens step by step:

1. Apex sees the `*` operator. Multiplication has higher precedence than addition, so it evaluates `3 * 4` first: result is `12`.
2. Then it evaluates `2 + 12`: result is `14`.

So `result` becomes `14`, not `20`. If you expected `20`, you were evaluating left to right: `2 + 3 = 5`, then `5 * 4 = 20`. But Apex follows math precedence rules, not simple left-to-right order.

The full precedence order for arithmetic operators is:

1. `*`, `/`, `%` — multiplication, division, and modulo happen first
2. `+`, `-` — addition and subtraction happen second

When two operators have the same precedence — like `*` and `/` — Apex evaluates from left to right:

```apex
result = 10 / 5 * 2  // (10 / 5) * 2
```

### Using Parentheses to Control Order
If you want to change the order of evaluation, use parentheses `()`. Anything inside parentheses is evaluated first:

```apex
result = (2 + 3) * 4
```

Now the steps are:

1. Parentheses first: `2 + 3 = 5`
2. Then multiplication: `5 * 4 = 20`

The result is `20`. Parentheses override the normal precedence rules, just like in math class. When in doubt, use parentheses — they make your intention clear and prevent subtle bugs.

```apex
a = (10 + 5) * 2        // 30
b = 10 + (5 * 2)        // 20
c = (10 - 3) / (2 + 1)  // 7 / 3 = 2.333...
```

### Combining Operators with Variables
You can build more complex expressions by combining multiple operators and variables:

```apex
price = 100
discount = 20
tax_rate = 0.07

final_price = (price - discount) * (1 + tax_rate)
```

Each step evaluates according to the precedence rules, with parentheses taking priority.

### Arithmetic Only Works with Numbers
One crucial rule: arithmetic operators work with numbers, and only numbers. You can add two numbers, subtract them, multiply them, divide them, take the remainder. But you cannot add a number to a string, or multiply a boolean by a table:

```apex
value = 10 + 5       // 15 — fine, both are numbers
value = "hello" + 5  // ERROR — can't add string to number
value = true * 3     // ERROR — can't multiply boolean by number
```

Apex is strict about this. It won't try to guess what you meant. If you write an arithmetic expression with non-number operands, Apex stops and tells you there's a problem. This is a good thing — it catches bugs early, before they cause confusing behavior later.

If you have a string like `"42"` and you want to do math with it, you need to convert it to a number first:

```apex
text = "42"
value = number(text)  // now value is the number 42
result = value + 8    // 50 — works because value is a number
```

We covered the `number()` function in the Built-in Functions section — this is exactly the kind of situation where it's essential.

### Whole Numbers and Decimals Together
When you combine a whole number and a decimal number in an arithmetic expression, the result is always a decimal:

```apex
5 + 3.5 = 8.5   // decimal result
10 / 4 = 2.5    // decimal result
7 * 2.0 = 14.0  // decimal result
```

Apex preserves the decimal part whenever it appears. You don't have to do anything special — it handles the conversion automatically.

## Comparison Operators
Arithmetic operators let you do math with numbers. But programs don't just calculate — they also *compare*. Is this price too high? Is this user old enough? Is this password correct? Comparison operators are the tools that answer these questions. They take two values, compare them, and give you a boolean result: either `true` or `false`.

### What Comparison Operators Do
A comparison operator looks at two values and asks a question about their relationship. The answer to that question is always a boolean — `true` if the comparison holds, `false` if it doesn't.

Here's the full set of comparison operators in Apex:

| Operator | Name                     | Example  |
|----------|--------------------------|----------|
| `==`     | Equal to                 | `5 == 5` |
| `!=`     | Not equal to             | `5 != 3` |
| `<`      | Less than                | `3 < 5`  |
| `>`      | Greater than             | `5 > 3`  |
| `<=`     | Less than or equal to    | `3 <= 3` |
| `>=`     | Greater than or equal to | `5 >= 5` |

Each of these produces a boolean value. You can store that result in a variable, use it in another expression, or — as you'll see in the next section — use it to make decisions.

### Equal To
The equal-to operator is written as two equals signs: `==`. It checks whether two values are exactly the same:

```apex
5 == 5   // true
10 == 3  // false
```

Why two equals signs? Because a single `=` is already taken — it's the assignment operator, used to put values into variables:

```apex
x = 5   // assignment: put 5 into x
x == 5  // comparison: is x equal to 5?
```

These look similar but do completely different things. The first *changes* a variable. The second *asks a question* about a variable. Mixing them up is a classic beginner mistake, so pay close attention to the difference.

**Comparing strings:**
The `==` operator works with strings too:

```apex
"hello" == "hello"  // true
"hello" == "world"  // false
```

Two strings are equal only if they contain exactly the same characters in exactly the same order. Case matters:

```apex
"Hello" == "hello"  // false — uppercase H vs lowercase h
```

**Comparing different types:**
When you compare values of different types with `==`, the answer is always `false`. A number and a string are never equal, even if they look similar:

```apex
5 == "5"        // false — number vs string
true == "true"  // false — boolean vs string
none == "none"  // false — none vs string
```

The type matters just as much as the value. A number `5` and a string `"5"` are fundamentally different things, so they're not equal.

**Comparing booleans:**
Booleans can be compared too:

```apex
true == true   // true
true == false  // false
```

**Comparing tables:**
Two tables are equal only if they're the *same* table — the same container, not just two containers with the same contents:

```apex
a = [1, 2, 3]
b = [1, 2, 3]
a == b  // false — two different tables
```

Even though `a` and `b` contain the same values, they're separate containers, so they're not equal. This distinction will matter more as you work with tables.

### Not Equal To
The not-equal-to operator is written as `!=`. It's the opposite of `==`: it gives `true` when the values are different, and `false` when they're the same:

```apex
5 != 3              // true
10 != 10            // false
"hello" != "world"  // true
true != false       // true
```

You can think of `!=` as asking "Are these different?" If yes, you get `true`. If no, you get `false`.

Different types are always not equal:

```apex
5 != "5"  // true — number vs string, always different
```

### Less Than and Greater Than
The less-than operator `<` and greater-than operator `>` work with numbers:

```apex
3 < 5   // true — 3 is less than 5
5 < 3   // false — 5 is not less than 3
10 > 5  // true — 10 is greater than 5
5 > 10  // false — 5 is not greater than 10
```

These comparisons are strict. `<` means strictly less than, and `>` means strictly greater than. The value itself is not included:

```apex
5 < 5  // false — 5 is not less than 5
5 > 5  // false — 5 is not greater than 5
```

For "less than *or equal*" and "greater than *or equal*," we have separate operators — coming next.

### Less Than or Equal To and Greater Than or Equal To
The `<=` operator checks whether a value is less than or equal to another. The `>=` operator checks whether a value is greater than or equal to another:

```apex
5 <= 5  // true — 5 is equal to 5
5 <= 6  // true — 5 is less than 6
5 <= 4  // false — 5 is neither less than nor equal to 4

5 >= 5  // true — 5 is equal to 5
5 >= 4  // true — 5 is greater than 4
5 >= 6  // false — 5 is neither greater than nor equal to 6
```

These are useful when you want to include the boundary value. For example, "you must be at least 18" means "age must be greater than or equal to 18":

```apex
age = 18
is_allowed = age >= 18  // true — 18 is allowed
```

If you used `>` instead, 18 would not be allowed:

```apex
is_allowed = age > 18  // false — 18 is not greater than 18
```

So `>=` and `<=` make a meaningful difference when the boundary value matters.

### Comparison Results Are Booleans
Every comparison operator produces a boolean result. This means you can assign comparison results to variables:

```apex
age = 25
is_adult = age >= 18       // true
is_teenager = age < 20     // false
is_exactly_25 = age == 25  // true
```

Each of these variables now holds a boolean — `true` or `false` — based on the comparison. This is incredibly useful. You can compute answers to questions once, store them, and use them later.

You can even compare the results of arithmetic:

```apex
price = 100
discount = 30
is_under_budget = (price - discount) < 80  // 70 < 80 → true
```

Here, Apex first does the arithmetic (`price - discount` becomes `70`), then does the comparison (`70 < 80` becomes `true`).

### Comparisons with Variables on Both Sides
So far, most examples have compared a variable to a literal value — like `age >= 18` where `18` is written directly in the code. But you can compare two variables just as easily:

```apex
my_age = 25
your_age = 30
am_i_older = my_age > your_age        // false — 25 is not greater than 30
are_we_same_age = my_age == your_age  // false
```

This works because Apex first looks up the values in both variables, then compares those values.

### Comparison Only Works with Compatible Types
You might have noticed that `<`, `>`, `<=`, and `>=` were only shown with numbers. That's because these four operators work exclusively with numbers. You cannot use them with strings, booleans, tables, or `none`:

```apex
"apple" < "banana"  // ERROR — can't compare strings with <
true > false        // ERROR — can't compare booleans with >
[] <= []            // ERROR — can't compare tables with <=
```

Apex won't try to guess what "less than" means for text or booleans. Those concepts only make sense for quantities, so Apex restricts these operators to numbers.

The equality operators `==` and `!=` are more flexible — they work with any type, as we saw earlier. You can compare strings, booleans, and tables for equality or inequality. But ordering comparisons (`<`, `>`, `<=`, `>=`) are strictly numeric.

### Operator Precedence
Comparison operators have lower precedence than arithmetic operators. This means arithmetic happens first, then comparison:

```apex
2 + 3 > 4
```

Apex first evaluates `2 + 3`, getting `5`. Then it evaluates `5 > 4`, getting `true`. You don't need parentheses for this — it happens naturally because arithmetic binds more tightly than comparison.

But if your expression is complex, parentheses make it clearer:

```apex
(2 + 3) > (4 * 1)  // 5 > 4 → true
```

This does the same thing but is easier to read.

Among comparison operators, equality (`==`, `!=`) has slightly lower precedence than ordering (`<`, `>`, `<=`, `>=`). In practice, you'll rarely write expressions that mix multiple comparison operators without parentheses, so this distinction rarely matters.

### Chaining Comparisons
One thing to note: you cannot chain comparisons the way you might in math. In math, you might write `5 < x < 10` to mean "x is between 5 and 10." In Apex, this doesn't work the way you'd expect:

```apex
5 < x < 10  // this evaluates left to right: (5 < x) < 10
```

To express "between," you'll need to combine two comparisons with a logical operator — which we'll cover in the next section. For now, know that each comparison is a standalone operation that compares exactly two values.

## Logical Operators
Comparison operators let you ask single questions: "Is this age greater than 18?" "Is this name equal to Alice?" But real decisions are rarely that simple. You often need to ask compound questions: "Is this person over 18 **and** do they have a license?" "Is today Saturday **or** is it a holiday?" "Is the user **not** banned?"

Logical operators are the tools that combine booleans into more complex conditions. They take boolean values as input and produce a boolean value as output. Since comparisons produce booleans, you can combine comparisons with logical operators to build up rich, nuanced conditions.

### The Three Logical Operators
Apex provides three logical operators:

| Operator | What It Does                   | Example                |
|----------|--------------------------------|------------------------|
| `and`    | Both sides must be true        | `(5 < 10) and (2 > 1)` |
| `or`     | At least one side must be true | `(2 > 1) or (2 < 1)`   |
| `not`    | Reverses the value             | `not true`             |

Let's explore each one.

### The AND Operator
The `and` operator combines two booleans and gives `true` only if **both** are `true`. If either side is `false` — or if both are — the result is `false`.

Here's the full truth table for `and`:

| Left    | Right   | Result  |
|---------|---------|---------|
| `true`  | `true`  | `true`  |
| `true`  | `false` | `false` |
| `false` | `true`  | `false` |
| `false` | `false` | `false` |

Think of `and` like a strict requirement. If you say "I'll go to the party if Alice comes **and** Bob comes," you'll only go when both of them show up. If either one is missing, you stay home.

**Using `and` with comparisons:**
The real power of `and` comes from combining comparisons:

```apex
age = 25
has_license = true
can_drive = (age >= 18) and (has_license == true)
```

Let's trace through this:

1. `age >= 18` evaluates to `true` (25 is at least 18)
2. `has_license == true` evaluates to `true` (the variable holds `true`)
3. `true and true` evaluates to `true`

So `can_drive` becomes `true`. If either condition had been false — say, `has_license` was `false` — then `can_drive` would be `false`.

```apex
age = 25
has_license = false
can_drive = (age >= 18) and (has_license == true)  // false
```

Now `true and false` gives `false`. The person is old enough but doesn't have a license, so they can't drive.

**A note on comparing booleans:**
You might notice that `has_license == true` is a bit verbose. Since `has_license` is already a boolean, you could just write `has_license` on its own. But remember from earlier: Apex requires conditions to be explicitly boolean, and there's nothing wrong with being explicit. Both of these work:

```apex
can_drive = (age >= 18) and (has_license == true)
can_drive = (age >= 18) and has_license
```

The second is shorter. The first is more obvious about what it's checking. Choose whichever reads better to you.

### The OR Operator
The `or` operator combines two booleans and gives `true` if **at least one** is `true`. It only gives `false` when both sides are `false`.

Here's the truth table for `or`:

| Left    | Right   | Result  |
|---------|---------|---------|
| `true`  | `true`  | `true`  |
| `true`  | `false` | `true`  |
| `false` | `true`  | `true`  |
| `false` | `false` | `false` |

Think of `or` like a flexible option. If you say "I'll go to the party if Alice comes **or** Bob comes," you'll go if at least one of them shows up. You only stay home if neither comes.

**Using `or` with comparisons:**
```apex
day = "Saturday"
is_holiday = false
can_relax = (day == "Saturday") or (is_holiday == true)
```

Let's trace through:

1. `day == "Saturday"` evaluates to `true` (the day is Saturday)
2. `is_holiday == true` evaluates to `false` (it's not a holiday)
3. `true or false` evaluates to `true`

So `can_relax` becomes `true`. Even though it's not a holiday, it's Saturday, and that's enough.

```apex
day = "Tuesday"
is_holiday = false
can_relax = (day == "Saturday") or (is_holiday == true)    // false
```

Now both sides are `false`: it's not Saturday, and it's not a holiday. So `false or false` gives `false`. No relaxing today.

### The NOT Operator
The `not` operator is different from `and` and `or`. It takes **one** boolean value — not two — and flips it. If the value is `true`, `not` makes it `false`. If it's `false`, `not` makes it `true`.

Here's the truth table for `not`:

| Value   | Result  |
|---------|---------|
| `true`  | `false` |
| `false` | `true`  |

```apex
not true   // false
not false  // true
```

Think of `not` as the word "isn't" or "doesn't." If `is_raining` is `true`, then `not is_raining` is `false` — because it's not the case that it isn't raining.

**Using `not` with comparisons:**
```apex
is_raining = false
can_walk = not is_raining    // true — it's not raining, so we can walk
```

Here, `is_raining` is `false`, so `not is_raining` is `true`. The variable `can_walk` becomes `true`.

```apex
is_raining = true
can_walk = not is_raining    // false — it's raining, so we can't walk
```

Now `is_raining` is `true`, so `not is_raining` is `false`. `can_walk` is `false`.

**A common use of `not`:**
`not` is often used to check that something is *not* the case:

```apex
user = none
has_user = not (user == none)  // false — user is none, so it's not the case that user exists
```

Wait, let's trace this carefully:

1. `user == none` evaluates to `true` (the user variable holds `none`)
2. `not true` evaluates to `false`

So `has_user` becomes `false`, which makes sense: if `user` is `none`, then there is no user, so `has_user` should be false.

### Combining Logical Operators
You can combine `and`, `or`, and `not` to build complex conditions. Just like with arithmetic, logical operators have a precedence order that determines how expressions are evaluated.

The precedence from highest to lowest is:

1. `not` — happens first
2. `and` — happens second
3. `or` — happens last

This means `not` binds most tightly, `and` next, `or` least tightly. Consider this expression:

```apex
true or false and false
```

Without precedence rules, you might read this left to right and get confused. But with precedence, `and` happens before `or`, so it's actually:

```apex
true or (false and false)
```

Let's evaluate:

1. `false and false` evaluates to `false`
2. `true or false` evaluates to `true`

So the whole expression is `true`.

If you wanted the `or` to happen first, you'd need parentheses:

```apex
(true or false) and false
```

Now:

1. `true or false` evaluates to `true`
2. `true and false` evaluates to `false`

So the expression is `false`. Different order, different result.

The full precedence order — including the operators from earlier sections — is:

1. `()` — parentheses
2. `*`, `/`, `%` — multiplication, division, modulo
3. `+`, `-` — addition, subtraction
4. `<`, `>`, `<=`, `>=` — ordering comparisons
5. `==`, `!=` — equality comparisons
6. `not` — logical NOT
7. `and` — logical AND
8. `or` — logical OR

When in doubt, use parentheses. They cost nothing and make your intention obvious.

## If Statements
So far, every line of code you've written has run from top to bottom, one after another. That's fine for simple calculations, but real programs need to make decisions. They need to do one thing if a condition is true, and another thing if it's false. That's where if statements come in.

Think of an if statement like a fork in the road. You stand at the fork, and you ask a yes-or-no question. If the answer is yes, you take the left path. If the answer is no, you take the right path (or just stay put). The question you ask is called a condition, and it must be something that can be answered with a boolean: either true or false.

In Apex, an if statement looks like this:

```apex
if condition
    // do something
```

The condition is an expression that evaluates to a boolean. It could be a comparison, like `age > 18`, or a logical combination. It cannot be a number or a string. Apex requires you to be explicit: you must write a comparison or a boolean variable compared to `true` or `false`. You cannot write `if x` and expect it to mean "if x is not zero" or "if x is not none". That's not allowed. You must write `if x > 0` or `if x != none` or whatever makes sense.

After the condition, you put an indented block of code. That block runs only if the condition is true. The indentation is four spaces. Apex uses indentation to know which lines belong to the if block.

Let's look at a simple example:

```apex
can_vote = none
age = 20
if age >= 18
    can_vote = true
```

After this code runs, `can_vote` is `true`. If `age` were 16, the condition `age >= 18` would be false, and the indented block would be skipped. `can_vote` would remain `none`.

Notice that the condition `age >= 18` is a comparison. It produces a boolean. That's exactly what Apex wants.

Now let's explore the different forms of if statements.

### If Statement
The simplest if statement has just one branch: the code that runs when the condition is true. If the condition is false, nothing happens.

Syntax:
```apex
if condition
    // code to run if condition is true
```

You can have as many lines as you want inside the block, as long as they are all indented by four spaces. For example:

```apex
temperature = 30
message = ""
advice = ""
if temperature > 25
    message = "It's hot outside"
    advice = "Drink plenty of water"
```

After this, `message` is `"It's hot outside"` and `advice` is `"Drink plenty of water"`. Both lines ran because the condition was true. If the condition were false, neither line would run.

Remember: the condition must be a boolean expression. You cannot write `if temperature` because `temperature` is a number, not a boolean. You must write a comparison. You also cannot write `if is_ready` if `is_ready` is a boolean variable. You must write `if is_ready == true` or `if is_ready == false`. Apex does not have truthy or falsy values.

Let's see an example with a boolean variable:

```apex
is_raining = true
action = ""
if is_raining == true
    action = "Take an umbrella"
```

Here, `is_raining == true` is a comparison that yields `true`. The block runs, and `action` becomes `"Take an umbrella"`. If `is_raining` were `false`, the block would be skipped.

You can also use logical operators to combine conditions. For example:

```apex
age = 25
has_license = true
can_drive = false
if age >= 18 and has_license == true
    can_drive = true
```

Here, both conditions must be true for the block to run. Since `age >= 18` is true and `has_license == true` is true, `can_drive` becomes `true`.

#### A Note on the Block
The block after an `if` is a **scope** — just like the body of a function or a loop. Variables you declare inside the block live only inside the block. When the block ends, they're gone.

```apex
import os

x = 5
if x < 10
    y = 42
    os.output(y)  // 42

os.output(y)  // ERROR — y is not defined here
```

The variable `y` is created inside the if block. It's visible only there. Once the if statement finishes, `y` no longer exists. This is a general rule in Apex: **indentation defines scope**. Every time you indent, you enter a new scope.

If you need a variable to survive past the if statement, declare it before the `if`:

```apex
import os

x = 5
y = 0
if x < 10
    y = 42
os.output(y)  // 42 — y survives
```

Now `y` is declared outside, and the assignment inside the block modifies the outer `y`. The value survives.

#### Multiple Conditions
You can combine any number of comparisons with logical operators. For example:

```apex
age = 25
has_license = true
is_sober = true
can_drive = false

if age >= 18 and has_license == true and is_sober == true
    can_drive = true
```

All three conditions must be true. If any one of them is false, the block is skipped. The `and` operator chains them together, and the whole expression is a boolean.

You can use `or` to allow alternatives:

```apex
day = "Saturday"
is_holiday = false
can_relax = false

if day == "Saturday" or is_holiday == true
    can_relax = true
```

Here, if either condition is true, the block runs. Since `day == "Saturday"` is true (and `is_holiday == true` is false), the block runs anyway, and `can_relax` becomes `true`.

And you can combine `and`, `or`, and `not`:

```apex
is_weekend = true
has_work = false
can_relax = false

if (is_weekend == true or has_work == false) and not (has_work == true)
    can_relax = true
```

Don't worry too much about this last example — it's just to show that you can combine as much as you need. Use parentheses when the logic gets complex; they cost nothing and make your intention obvious.

### Else-If Statement
Sometimes you have more than two possibilities. You want to check a second condition if the first one is false, and a third condition if the second is false, and so on. That's what `else if` is for.

Syntax:
```apex
if condition1
    // code if condition1 is true
else if condition2
    // code if condition1 is false and condition2 is true
else if condition3
    // code if condition1 and condition2 are false, and condition3 is true
```

You can have as many `else if` blocks as you need. Each one is checked in order, from top to bottom. As soon as one condition is true, its block runs, and all the remaining `else if` and `else` blocks are skipped.

Let's look at an example that assigns a grade based on a score:

```apex
score = 85
grade = none

if score >= 90
    grade = "A"
else if score >= 80
    grade = "B"
else if score >= 70
    grade = "C"
```

After this code runs, `grade` is `"B"`. Let's trace through:
- `score >= 90` is false (85 is not >= 90), so we skip the first block.
- `score >= 80` is true, so we run that block and set `grade = "B"`.
- The remaining `else if` blocks are skipped.

If `score` were 95, `grade` would be `"A"`. If `score` were 75, `grade` would be `"C"`. If `score` were 65, none of the conditions would be true, and `grade` would remain `none`.

Notice that each `else if` is on the same indentation level as the original `if`. The blocks are indented four spaces. This indentation tells Apex which code belongs to which branch.

**Important:** The conditions are checked in order. Once a condition is true, the rest are ignored. So you should order your conditions from most specific to least specific, or from highest to lowest, as in the grade example.

#### Order Matters
Here's an example where the wrong order causes a bug:

```apex
score = 85
grade = none

if score >= 70
    grade = "C"
else if score >= 80
    grade = "B"
else if score >= 90
    grade = "A"
```

If you run this with `score = 85`, the first condition `score >= 70` is true (85 is at least 70). So `grade` becomes `"C"`. The remaining `else if` blocks are skipped, and the fact that 85 is also >= 80 never gets a chance. The result is wrong.

The problem is that the conditions aren't specific enough. The first one catches too many cases. By ordering from highest to lowest — `>= 90` first, then `>= 80`, then `>= 70` — you make sure each score lands in the correct bracket.

Always think about the ordering when you write an `else if` chain. Ask yourself: "Could an earlier condition steal a case meant for a later one?"

#### Each Branch Is Its Own Scope
Just like with a plain `if`, each branch in an `else if` chain creates its own scope. Variables declared inside a branch are local to that branch. You can even reuse the same variable name in different branches, and they won't conflict.

```apex
import os

score = 85

if score >= 90
    grade = "A"
    os.output(grade)
else if score >= 80
    grade = "B"
    os.output(grade)
else
    grade = "C"
    os.output(grade)

os.output(grade)  // ERROR — grade is not defined here
```

The variable `grade` is declared inside each branch. Each branch has its own `grade`. None of them are visible after the entire chain is finished. If you want `grade` to survive, declare it outside the chain.

### Else Statement
The `else` block runs when none of the previous conditions were true. It's the catch-all. You can have at most one `else`, and it must be the last branch.

Syntax:
```apex
if condition
    // code if condition is true
else
    // code if condition is false
```

You can combine `else if` and `else`:

```apex
if condition1
    // code if condition1 is true
else if condition2
    // code if condition1 is false and condition2 is true
else
    // code if all conditions are false
```

Let's extend the grade example with an `else`:

```apex
score = 65
grade = none

if score >= 90
    grade = "A"
else if score >= 80
    grade = "B"
else if score >= 70
    grade = "C"
else
    grade = "F"
```

Now, if `score` is 65, none of the `if` or `else if` conditions are true, so the `else` block runs and `grade` becomes `"F"`. If `score` were 75, `grade` would be `"C"` and the `else` would be skipped.

The `else` block has no condition. It simply runs when all previous conditions were false. It's a good way to handle the "everything else" case.

#### When to Use Else
Not every `if` chain needs an `else`. If there's nothing meaningful to do when all conditions fail, you can leave it out. Execution just continues with the code after the chain.

Use `else` when:
- You want to handle the "everything else" case explicitly.
- You want to make sure at least one branch always runs.
- You want the reader to see that all possibilities are covered.

If you don't need it, skip it. Simpler code is usually better.

#### A Common Pattern: Validation
A common pattern is to use `if`/`else if`/`else` for validation — checking a series of conditions and reporting the first one that fails.

```apex
import os

username = "ab"

if string.length(username) < 3
    os.output("Username too short")
else if string.length(username) > 20
    os.output("Username too long")
else
    os.output("Username is valid")
```

Here, the first condition fails (length is 2, which is < 3), so the first branch runs and prints `"Username too short"`. The remaining branches are skipped. If the username were longer than 3 and shorter than 20, the `else` branch would run.

This pattern — checking each rule in turn and reporting the first failure — is very common. It's clear, easy to read, and easy to extend.

#### No Else Needed for Simple Cases
If you have a single `if` and nothing needs to happen when the condition is false, don't add an `else`. Just let the code continue.

```apex
balance = 100
if balance < 0
    balance = 0
os.output(balance)
```

Here, if `balance` is negative, we clamp it to zero. If it's already non-negative, nothing changes. There's no need for an `else` — the "do nothing" case is handled by simply not running the block.

### Ternary Expression
The ternary expression is a shorthand for a simple if-else that chooses between two values. It's an expression, so it produces a value. You can use it anywhere you can use a value, such as on the right side of an assignment.

The syntax is a bit different from some other languages. In Apex, you write:

```apex
value_if_true if condition else value_if_false
```

Notice the order: first the value for when the condition is true, then the word `if`, then the condition, then the word `else`, then the value for when the condition is false.

For example:

```apex
age = 20
status = "adult" if age >= 18 else "minor"
```

After this, `status` is `"adult"`. If `age` were 16, `status` would be `"minor"`.

You can use the ternary anywhere you need to choose between two values. For example:

```apex
price = 100
discount = 20
final_price = price - discount if discount > 0 else price
```

Here, `final_price` becomes 80 because `discount > 0` is true, so the expression before `if` is used (`price - discount`). If `discount` were 0, `final_price` would be `price`.

The condition in a ternary must be a boolean expression, just like in a regular if statement. The two values can be of any type, but they should be compatible for the context.

#### Ternary vs. If Statement
The ternary is not a replacement for an `if` statement. It's a tool for a specific situation: choosing between two values. It cannot contain multiple statements, and it cannot be used when you need to do different things (rather than produce different values).

Use a ternary when:
- You need to pick one of two values based on a condition.
- The condition is short and simple.
- Both branches are simple expressions, not multi-statement blocks.

Use an `if` statement when:
- You need to run multiple statements in one or both branches.
- The logic requires more than two branches.
- The condition is complex enough that it deserves its own line.

#### Cannot Chain Ternaries
You cannot chain ternary expressions. This is not allowed:

```apex
// NOT ALLOWED
grade = "A" if score >= 90 else "B" if score >= 80 else "C"
```

Apex requires you to use a regular `if`/`else if`/`else` statement for cases that involve more than two possibilities. The ternary is strictly a two-way choice.

```apex
// CORRECT WAY
if score >= 90
    grade = "A"
else if score >= 80
    grade = "B"
else
    grade = "C"
```

This restriction is deliberate. The ternary is meant to be short and readable. Chaining them turns them into a hard-to-read puzzle. If you have more than two branches, use the statement form.

#### A Word on Apex's Ternary Order
If you're coming from another language, you might be used to writing the condition first: `condition ? value_if_true : value_if_false`. Apex flips this around. The value comes first, then the condition, then the alternative.

```apex
// Apex
status = "adult" if age >= 18 else "minor"
```

Read it out loud: "adult if age is at least 18, else minor." That reads naturally, like English. The order is a design choice to make the expression easier to read aloud. Once you get used to it, you may find it clearer than the traditional form.

#### Boolean Conditions Need Explicit Comparison
As with all conditions in Apex, the ternary condition must be an explicit boolean. You cannot write:

```apex
status = "yes" if is_active else "no"  // ERROR — is_active is a boolean, but not a comparison
```

You must write:

```apex
status = "yes" if is_active == true else "no"
```

This is consistent with the rest of the language. Apex never treats a bare boolean as a condition; it always requires a comparison (`== true`, `== false`, `!= none`, etc.).

#### A Practical Example
Here's an example that puts everything together. A function that returns a friendly greeting based on the time of day:

```apex
import os

function greeting(hour)
    time_of_day = "morning" if hour < 12 else "afternoon" if hour < 18 else "evening"
    return "Good {time_of_day}"
```

Wait — that example chains ternaries, which Apex doesn't allow. Let's rewrite it with an if statement:

```apex
import os

function greeting(hour)
    time_of_day = ""
    if hour < 12
        time_of_day = "morning"
    else if hour < 18
        time_of_day = "afternoon"
    else
        time_of_day = "evening"
    return "Good {time_of_day}"

os.output(greeting(10))  // Good morning
os.output(greeting(15))  // Good afternoon
os.output(greeting(20))  // Good evening
```

This is the correct way to handle three or more possibilities. The ternary works for two; the if statement works for any number.

## Match / Case
Sometimes you have a single value that you need to compare against many different possibilities. You could write a long chain of `if` and `else if` statements, but that gets messy quickly. Apex gives you a cleaner tool for this exact situation: the `match` statement.

Think of `match` as a specialized decision-maker. You give it one value — the subject — and then you list a series of constant patterns. Apex checks the subject against each pattern in order. As soon as it finds a match, it runs the corresponding block of code and then skips the rest of the `match`. It's like a multi-way fork in the road, but much more readable than a pile of `else if`s.

`match` is not an expression. It doesn't produce a value you can assign. It's a statement, just like `if`. You use it when you want to *do* different things based on a value, not when you want to compute a result.

### Match Statement
The `match` keyword is followed by the value you want to check — the subject. Then you write an indented block containing `case` branches. Each `case` has a constant pattern, and below it (indented further) is the code that runs when the subject equals that pattern.

Syntax:
```apex
match subject
    case pattern1
        // code to run if subject equals pattern1
    case pattern2
        // code to run if subject equals pattern2
    // ... more cases ...
```

Let's look at a simple example. Suppose you have a numeric status code and you want to set a message based on it.

```apex
status = 404
message = ""

match status
    case 200
        message = "OK"
    case 404
        message = "Not Found"
    case 500
        message = "Server Error"
```

After this runs, `message` is `"Not Found"`. Here's what happens:
- Apex looks at `status`, which is `404`.
- It checks `case 200`: 404 is not 200, so it moves on.
- It checks `case 404`: 404 equals 404, so it runs the block `message = "Not Found"`.
- It then skips the remaining cases (there's only `case 500` left, which is ignored).

If `status` were `200`, `message` would be `"OK"`. If `status` were `500`, `message` would be `"Server Error"`. If `status` were something else, like `302`, none of the cases would match, and `message` would stay `""`.

Notice the indentation. The `match` line is at the current indentation. The `case` lines are indented four spaces. The code inside each case is indented another four spaces. This is how Apex knows which code belongs to which case. Just like with `if`, indentation defines the blocks.

You can have as many `case` branches as you need. They are checked from top to bottom. The first one that matches wins, and the rest are ignored.

#### A Note on the Block
Just like with `if`, each `case` body is its own **scope**. Variables declared inside a case live only inside that case. When the case ends, they're gone.

```apex
import os

code = 200

match code
    case 200
        message = "OK"
        os.output(message)
    case 404
        message = "Not Found"  // same name, different case — OK
        os.output(message)

os.output(message)  // ERROR — message is not defined here
```

The variable `message` is declared inside each case. Each case has its own `message`. None of them are visible after the `match` is finished. If you want a variable to survive past the match, declare it before the `match`.

```apex
import os

code = 200
message = ""

match code
    case 200
        message = "OK"
    case 404
        message = "Not Found"

os.output(message)  // OK — message was declared outside
```

Now `message` is declared outside the `match`, so the assignment inside the case modifies the outer variable. The value survives.

This is consistent with the rest of Apex: **indentation defines scope**. Every time you indent, you enter a new scope. Every time you dedent, you leave it.

#### Cases Are Checked from Top to Bottom
The order of cases matters. Apex checks them one at a time, starting from the top. As soon as one matches, its block runs, and the rest are skipped.

This means that if two patterns could match the same subject, only the first one will ever run. Consider this:

```apex
grade = 85
result = ""

match grade
    case 85
        result = "exact"
    case 85
        result = "duplicate"
```

The subject is `85`. The first case matches, and `result` becomes `"exact"`. The second case is never reached. It's not an error to have duplicate patterns, but it's pointless — the second one is dead code.

More commonly, the order matters when patterns overlap in *meaning*, not in literal value. For instance, if you're matching against status codes and you have a case for `200` and a case for `200`, the second one is unreachable. But if you're matching against strings and you have a case for `"hello"` and a case for `"hello world"`, they're different patterns, and both can match. Which one runs depends on which one appears first.

Apex will actually warn you when a case can never match. If your patterns are mutually exclusive (as they should be for clean code), you won't see any warnings.

### Case Patterns
A pattern is the value you compare against. It must be a **constant** — something that never changes. You cannot use a variable as a pattern, because the whole point of `match` is to compare against fixed, known values.

The allowed constant patterns are:
- **Number literals**: like `42`, `3.14`, or negative numbers like `-1`.
- **String literals**: like `"hello"`, `"error"`, or `""` (empty string).
- **Boolean literals**: `true` or `false`.
- **None**: the special value `none`.

Here's an example with string patterns:

```apex
command = "quit"
action = ""

match command
    case "start"
        action = "Starting..."
    case "stop"
        action = "Stopping..."
    case "quit"
        action = "Goodbye!"
```

After this, `action` is `"Goodbye!"`. The subject `command` is a string, and the patterns are string literals. They match by exact text. Case matters: `"Start"` would not match `"start"`.

You can also use booleans:

```apex
is_enabled = false
status = ""

match is_enabled
    case true
        status = "Enabled"
    case false
        status = "Disabled"
```

Here, `status` becomes `"Disabled"`.

And you can match against `none`:

```apex
result = none
message = ""

match result
    case none
        message = "No result"
    case 0
        message = "Zero"
```

Since `result` is `none`, it matches `case none`, and `message` becomes `"No result"`.

Negative numbers are allowed too:

```apex
temperature = -5
feeling = ""

match temperature
    case -10
        feeling = "Freezing"
    case -5
        feeling = "Very cold"
    case 0
        feeling = "Cold"
```

Here, `feeling` becomes `"Very cold"`.

#### Patterns Must Match the Subject's Type
The type of the pattern must match the type of the subject. You cannot match a number against a string pattern. If you try, Apex will warn you that the case can never match.

```apex
value = 42
result = ""

match value
    case "42"           // WARNING — string pattern, number subject
        result = "string"
    case 42
        result = "number"
```

The first case can never match, because the subject is a number and `"42"` is a string. Apex will tell you this. The second case works fine.

So if your subject is a number, all patterns must be numbers. If it's a string, all patterns must be strings. If it's a boolean, all patterns must be booleans. And so on.

#### Constants Only, No Variables or Expressions
Patterns must be literal constants. You cannot use a variable, a function call, or any expression as a pattern.

```apex
key = 42
value = 10
result = ""

match value
    case key            // ERROR — key is a variable, not a constant
        result = "matched key"
    case 10
        result = "matched ten"
```

The `case key` line is not allowed. Apex requires the pattern to be a known value at the time the program is compiled. A variable could change at runtime, and that would make the match unpredictable.

Similarly, you cannot use an expression:

```apex
match value
    case 40 + 2         // ERROR — expression, not a constant literal
        result = "matched 42"
```

You could write `case 42` directly, but not `case 40 + 2`.

#### Empty String and Zero Are Valid Patterns
An empty string `""` is a valid string pattern. Zero is a valid number pattern. They are not the same as `none`.

```apex
text = ""
result = ""

match text
    case ""
        result = "empty string"
    case none
        result = "none"
```

Since `text` is `""` (a string with zero characters), the first case matches, and `result` becomes `"empty string"`. If `text` were `none`, the second case would match instead.

This is a useful distinction to keep in mind. An empty string is still a string. `none` is not a string at all.

### Default Case
What if none of the patterns match? You can provide a default case that runs when nothing else matches. A default case is written as `case` with no value after it. It's like the `else` in an if-else chain.

The default case must be the **last** case in the `match`. You can only have one default case.

Example:

```apex
code = 302
description = ""

match code
    case 200
        description = "OK"
    case 404
        description = "Not Found"
    case
        description = "Unknown status"
```

After this, `description` is `"Unknown status"` because `302` didn't match `200` or `404`, so the default case ran.

If you omit the default case and no pattern matches, then the `match` statement simply does nothing. Execution continues with the code after the `match`. That might be fine if you only care about specific values. But if you want to handle "everything else," use a default case.

```apex
code = 302
description = "initial"

match code
    case 200
        description = "OK"
    case 404
        description = "Not Found"

// No default. Nothing matches 302, so description stays "initial"
```

Here, since no case matched and there's no default, `description` remains `"initial"`. The `match` simply did nothing.

#### The Default Case Must Be Last
The default case must come last. If you put any case after it, Apex will report an error. That's because once you have a default, any case below it would be unreachable — the default would always run first.

```apex
match value
    case 1
        // ...
    case                 // default
        // ...
    case 2               // ERROR — case after default
        // ...
```

This rule exists to prevent mistakes. If you could put cases after the default, you might accidentally think they're reachable when they're not. Apex stops you before that happens.

#### Only One Default Case
You can have at most one default case. Two defaults would be ambiguous — which one should run when nothing matches? So Apex allows only one.

```apex
match value
    case 1
        // ...
    case                 // default
        // ...
    case                 // ERROR — second default
        // ...
```

If you have two defaults, Apex will report an error on the second one.

#### When to Use a Default Case
Use a default case when you want to handle the "everything else" scenario. This is common when the subject can take on many values and you only care about a few specific ones.

For example, if you're processing commands and you handle a few known ones, the default catches typos or unsupported commands:

```apex
import os

command = "restart"
handled = true

match command
    case "start"
        os.output("Starting...")
    case "stop"
        os.output("Stopping...")
    case
        handled = false
        os.output("Unknown command")
```

Since `"restart"` isn't `"start"` or `"stop"`, the default runs, and `handled` becomes `false`.

You don't always need a default. If your cases cover every possible value (for a boolean subject, for instance), a default is redundant. If you're fine with "do nothing" when nothing matches, skip the default.

### Rules and Restrictions
To use `match` correctly, keep these rules in mind:

#### 1. Subject Type
The subject must be a `number`, `string`, `boolean`, or `none`. You cannot match against a table or any other complex type.

```apex
t = [1, 2, 3]

match t              // ERROR — table subject not allowed
    case 1
        // ...
```

Apex will report an error saying the subject must be a number, string, boolean, or none. Tables and functions are not allowed as subjects.

#### 2. Pattern Type
Each pattern must be a constant of the same type as the subject. You cannot mix types.

```apex
value = 42

match value
    case "42"        // WARNING — string pattern, number subject
        // ...
    case 42
        // ...
```

Apex will warn you that the first case can never match. It's not a hard error, but it's a bug in your code, and Apex helps you see it.

#### 3. Constants Only
Patterns must be literal constants. You cannot use variables, expressions, or function calls.

```apex
x = 42

match 10
    case x           // ERROR — x is a variable
        // ...
    case 10
        // ...
```

The `case x` line is not allowed. Use only literal values.

#### 4. Order Matters
Cases are checked from top to bottom. The first matching case wins. Once a case runs, the rest of the `match` is skipped. There is no fall-through like in some other languages' switch statements.

#### 5. Default Case
You may have at most one default case, written as `case` with no value. It must be the last case. If no case matches and there is no default, the `match` does nothing.

#### 6. Scope
Each `case` body has its own scope. Variables declared inside a case are local to that case and are not visible after the `match`. You can reuse the same variable name in different cases without conflict.

#### 7. Not an Expression
`match` is a statement, not an expression. It does not produce a value. You cannot write `x = match ...` or use it inside another expression.

```apex
// NOT ALLOWED
result = match value
    case 1
        // ...
```

If you need to compute a value based on a set of cases, use an if-else chain instead. The ternary won't help either — it only handles two branches. For multiple branches that produce a value, the if/else if/else statement is the right tool.

#### 8. No Tables
Tables cannot be used as subjects or patterns. Only the four simple types are allowed. If you need to branch based on a table, you'd have to extract a value from the table first, then match on that value.

#### 9. No Range Patterns
Apex `match` does not support range patterns like `case 1..10`. Each pattern is a single, exact value.

```apex
score = 85

match score
    case 90              // exact value only
        // ...
    case 1..89           // ERROR — ranges not supported
        // ...
```

If you need range checks, use an if-else chain with comparison operators.

```apex
score = 85
grade = ""

if score >= 90
    grade = "A"
else if score >= 80
    grade = "B"
else
    grade = "C"
```

This is the right tool for ranges. `match` is for exact values.

#### 10. No Compound Patterns
Apex `match` does not support compound patterns like `case 1, 2, 3` or `case 1 or 2`. Each case handles exactly one value.

```apex
value = 2

match value
    case 1, 2, 3         // ERROR — compound patterns not supported
        // ...
```

If you want to match several values to the same block, you would need to write each case separately, or use an if-else chain.

```apex
// Alternative: use if-else if you need to group values
if value == 1 or value == 2 or value == 3
    // ...
```

#### 11. No Fallthrough
Unlike C's `switch`, Apex's `match` has no fallthrough. Once a case matches, its block runs, and the `match` ends. You don't need a `break` statement.

```apex
match value
    case 1
        // only this block runs
    case 2
        // this is skipped if case 1 matched
```

There's no way to accidentally fall through from one case to the next. This is one of the reasons `match` is cleaner than `switch` in many other languages.

#### 12. Empty Case Bodies Are Allowed
A case body can be empty. If the subject matches and the case has no code, nothing happens. This is unusual but not an error.

```apex
match value
    case 1
    case 2
        // code for case 2
```

Here, if `value` is 1, the first case matches, its empty body runs (nothing happens), and the `match` ends. The second case is never reached. So this is different from C's `switch` fallthrough — Apex stops at the first match, empty or not.

If you actually want "do nothing for 1, do something for 2," you'd write:

```apex
match value
    case 1
        none                // explicitly do nothing (a single expression statement)
    case 2
        // code for case 2
```

Or, more commonly, you'd just include the "do nothing" case in a grouped if statement.

### Putting It Together
`match` is a powerful way to keep your code clean when you have many fixed options to check. It's especially handy for things like status codes, command strings, or simple state machines.

Here's a complete example that puts everything together:

```apex
import os

function describe_day(day)
    description = ""
    match day
        case "Monday"
            description = "Start of the work week"
        case "Friday"
            description = "Almost the weekend"
        case "Saturday"
            description = "Weekend!"
        case "Sunday"
            description = "Rest day"
        case
            description = "Just a regular day"
    return description

os.output(describe_day("Saturday"))   // Weekend!
os.output(describe_day("Wednesday"))  // Just a regular day
os.output(describe_day(""))           // Just a regular day (empty string doesn't match)
```

The function takes a day name. It matches against four known days and falls through to a default for anything else. The result is a short description.

#### When to Use Match vs. If
Use `match` when:
- You're comparing one value against many fixed possibilities.
- Each possibility is a constant (number, string, boolean, or none).
- You want the code to be clean and readable.

Use `if`/`else if`/`else` when:
- You need ranges or comparisons (like `score >= 90`).
- Your conditions involve different subjects (like `age > 18 and has_license == true`).
- You need complex boolean logic.

Both tools have their place. `match` is not better than `if` — it's just more specialized. For the specific case of "one value, many constants," `match` is clearer. For everything else, `if` is the right choice.

#### A Reminder About Comparison Semantics
When `match` compares the subject to a pattern, it uses the same comparison rules as `==`. That means:

- Numbers are compared by value: `42` matches `42`.
- Strings are compared by content: `"hello"` matches `"hello"`, not `"Hello"` (case matters).
- Booleans are compared by value: `true` matches `true`, `false` matches `false`.
- `none` matches `none`.
- Different types never match: `42` never matches `"42"`.
- Tables and functions cannot be used at all.

So if you're ever unsure whether a pattern will match, think about what `subject == pattern` would produce. If `==` gives `true`, the case matches. If `==` gives `false`, it doesn't.

Now you have another tool in your decision-making toolkit. In the next section, we'll learn how to repeat code with loops.

## For Loops
Programs often need to repeat the same action many times. You might want to count from one to ten, process every item in a table, or keep asking for input until the user types the right thing. Writing the same code over and over is not an option — it would be tedious and error-prone. That's where loops come in.

A loop is a way to tell Apex: "Do this block of code again and again, according to these rules." Apex gives you a single keyword — `for` — with three different forms, each suited to a different kind of repetition:

- **Counter**: when you know the exact range of numbers you want to walk through.
- **Table iteration**: when you want to visit every value inside a table, one by one.
- **Condition**: when you don't know how many times you'll repeat, but you know when to stop.

All three use `for`, and all three use the same indentation rule you already know: the loop body is a block indented by four spaces.

Before we look at each form, one important rule: **a loop creates its own scope**. Any variable you declare inside the loop body — including the loop variable itself — exists only inside that loop. Once the loop finishes, those variables are gone. You cannot use them afterward. We'll see this in action as we go.

Let's start with the most common form: the counter.

### For Counter
The counter form is for when you want to count. You give the loop a variable, a starting number, and an ending number. Apex runs the body once for each number in that range, including the end value.

Syntax:
```apex
for variable = start, end
    // code to run for each value
```

The loop variable takes on each value in turn: `start`, then `start + 1`, then `start + 2`, and so on, until it reaches `end`. At each step, the body runs. When the variable would go past `end`, the loop stops.

Here's a simple example that counts from 1 to 5:

```apex
result = ""
for i = 1, 5
    result = "{result}{i}"
```

After this code runs, `result` is `"12345"`. Let's trace through it:
- `i` starts at 1. The body runs: `result` becomes `"1"`.
- `i` becomes 2. The body runs: `result` becomes `"12"`.
- `i` becomes 3. The body runs: `result` becomes `"123"`.
- `i` becomes 4. The body runs: `result` becomes `"1234"`.
- `i` becomes 5. The body runs: `result` becomes `"12345"`.
- `i` would become 6, which is greater than 5, so the loop stops.

Notice the string interpolation `"{result}{i}"`. It builds up the result one digit at a time. Also notice that `result` is declared **outside** the loop, so it survives after the loop ends. But `i` is declared **inside** the loop header, so it only exists during the loop. If you tried to use `i` after the loop, Apex would report an error.

What if the start is greater than the end? For example:

```apex
result = ""
for i = 5, 1
    result = "{result}{i}"
```

Here, `i` starts at 5, which is already greater than the end value 1. The loop never runs. `result` stays `""`. This is not an error — it's just a loop with zero iterations.

#### Counting Down with a Step
By default, the counter increases by 1 each time. But you can add a third number — the **step** — to control how much it changes.

Syntax:
```apex
for variable = start, end, step
    // code
```

The step can be any number. If it's positive, the loop counts upward. If it's negative, the loop counts downward.

Counting upward by 2:
```apex
result = ""
for i = 0, 10, 2
    result = "{result}{i}"
```

After this, `result` is `"0246810"`. `i` takes the values 0, 2, 4, 6, 8, 10. When it would become 12 (greater than 10), the loop stops.

Counting downward:
```apex
result = ""
for i = 5, 1, -1
    result = "{result}{i}"
```

Here, `result` becomes `"54321"`. `i` takes 5, 4, 3, 2, 1, then would become 0, which is less than 1, so the loop stops.

If you use a negative step, the start value should be greater than the end value for the loop to run at all. If you swap them and still use a negative step, the loop won't run:

```apex
result = ""
for i = 1, 5, -1     // start 1, end 5, step -1
    result = "{result}{i}"
```

The step is negative, so the loop counts downward. But the start value (1) is less than the end value (5). Counting down from 1 never reaches 5 — it goes further and further away. So the loop doesn't run. `result` stays `""`.

**The step cannot be zero.** If you write `for i = 1, 10, 0`, Apex will report an error. A step of zero would mean the loop variable never changes, so the loop would never end — that's not allowed.

You can also use decimals as steps, but be careful. Floating-point arithmetic can introduce tiny rounding errors. For example:

```apex
result = ""
for i = 0, 2, 0.5
    result = "{result}{i} "
```

After this, `result` is `"0 0.5 1 1.5 2 "`. The loop counts from 0 by half-steps. But if you tried to use `0.1` as a step, you might find that accumulated rounding errors make the loop either run one extra time or one time too few. For most counting tasks, whole-number steps are what you want.

#### The Loop Variable Is a Number
The loop variable in a counter loop is always a number. You can use it in arithmetic, compare it, print it — anything you'd do with a number.

```apex
sum = 0
for i = 1, 5
    sum = sum + i
```

After this, `sum` is 15 (because 1+2+3+4+5 = 15). The loop variable `i` was used as a number in the expression `sum + i`.

If you wanted to compute the factorial of a number with a loop, you could do:

```apex
function factorial(n)
    result = 1
    for i = 1, n
        result = result * i
    return result
```

Calling `factorial(5)` returns 120.

#### Using a Decimal Start or End
The start and end values don't have to be whole numbers. You can use decimals:

```apex
result = ""
for i = 0.5, 2.5, 0.5
    result = "{result}{i} "
```

After this, `result` is `"0.5 1 1.5 2 2.5 "`. The loop runs for each value from 0.5 to 2.5, stepping by 0.5.

The start and end values are evaluated once, at the beginning of the loop. They are not re-evaluated on each iteration. So if you write:

```apex
end = 10
result = ""
for i = 1, end
    end = 100
    result = "{result}{i}"
```

The loop still stops at 10, because that's the value `end` had when the loop began. Changing `end` inside the loop has no effect on the loop's stop condition.

This is important. The `end` value is captured once, and the loop runs according to that captured value. Same for the step:

```apex
step = 1
result = ""
for i = 1, 5, step
    step = 2
    result = "{result}{i}"
```

The loop runs 1, 2, 3, 4, 5 (step 1), not 1, 3, 5. The step is captured once at the start.

#### The Loop Variable Cannot Be Modified Inside the Loop
You might wonder: what happens if I assign a new value to the loop variable inside the loop?

```apex
result = ""
for i = 1, 5
    i = 100
    result = "{result}{i}"
```

This doesn't work the way you might think. The loop variable `i` is controlled by the loop machinery. Assigning to it inside the body is confusing and can lead to undefined behavior. Apex manages the loop variable itself. Don't write code that assigns to the loop variable — it's a bug waiting to happen.

If you need a variable that you can modify, use a different name:

```apex
result = ""
for i = 1, 5
    temp = i * 2
    result = "{result}{temp}"
```

Now `temp` is a normal variable that you can modify freely. `i` stays under the loop's control.

#### Nested Counter Loops
You can put a loop inside another loop. This is called **nesting**. When you do, each loop has its own variable and its own scope.

```apex
result = ""
for i = 1, 2
    for j = 1, 2
        result = "{result}{i}{j} "
```

After this, `result` is `"11122122 "`. Let's trace through:
- `i` is 1. Inner loop runs with `j` from 1 to 2:
  - `j = 1`: result += "11 "
  - `j = 2`: result += "12 "
- `i` is 2. Inner loop runs again with `j` from 1 to 2:
  - `j = 1`: result += "21 "
  - `j = 2`: result += "22 "

The total is `"11122122 "`.

Notice that the inner loop runs completely for each iteration of the outer loop. This is the key idea of nesting. The outer loop controls the inner loop.

Nested loops are useful for working with grids, matrices, and any data that has multiple dimensions. For example, a chessboard is an 8×8 grid:

```apex
for row = 1, 8
    for col = 1, 8
        // process cell at (row, col)
```

Each cell gets its own processing. This pattern is extremely common.

But be careful with nesting. If you nest too many loops, the number of iterations grows fast. Two nested loops with 100 iterations each means 10,000 total iterations. Three nested loops with 100 iterations each means 1,000,000. The computer is fast, but it's not infinitely fast. If your nested loops take too long, look for ways to flatten them or reduce the range.

### For Table Iteration
Tables hold many values. Often you want to do something with each one — print it, add it to a total, check if it matches some condition. The table iteration form of `for` lets you visit every value in a table, one at a time.

Syntax:
```apex
for variable in table
    // code to run for each value
```

The loop variable takes on each **value** from the table. Notice I said value, not key. Apex gives you the values directly. You don't need to worry about positions or keys unless you want to.

Here's an example:

```apex
fruits = ["apple", "banana", "cherry"]
result = ""
for fruit in fruits
    result = "{result}{fruit} "
```

After this, `result` is `"apple banana cherry "`. The loop visits each string in the table, in order, and appends it to `result` followed by a space.

The loop variable `fruit` is a new variable, local to the loop. It changes on each iteration. You can name it whatever you like — `fruit`, `item`, `value`, `x`. Just pick a name that describes what the values are.

You can iterate over tables of any type — numbers, strings, booleans, even tables inside tables. For example:

```apex
numbers = [10, 20, 30, 40]
total = 0
for n in numbers
    total = total + n
```

After this, `total` is `100`. The loop adds each number to the running total.

#### Iterating Over Key-Value Tables
Now consider a key-value table:

```apex
user = ["name" = "Alice", "age" = 30, "city" = "Dubai"]
result = ""
for value in user
    result = "{result}{value} "
```

Here, `result` becomes `"30 Dubai Alice "` (or some other order — key-value tables do not guarantee the order in which values are visited). The loop visits the values `"Alice"`, `30`, and `"Dubai"`, but the order depends on the internal layout of the table.

This is important: **key-value tables do not preserve insertion order.** If you insert `"name"` first and `"age"` second, you might get `"age"` before `"name"` when iterating. If you need a specific order, you should sort or restructure your data first.

The key thing to remember: **for table iteration gives you the values, not the keys**. If you want the keys, Apex provides ways to get them, but that's a topic for later.

#### Iterating Over Mixed Tables
If a table has both positional values and key-value pairs, iteration visits all of them. But the order is not guaranteed for the key-value part.

```apex
mixed = ["first", "second", "name" = "Alice"]
result = ""
for v in mixed
    result = "{result}{v} "
```

After this, `result` might be `"first second Alice "` — the positional values come first, and then the key-value values. Or it might not, depending on the implementation. In practice, positional values are visited in order, and then key-value values are visited in their own internal order.

The takeaway: don't rely on the order of iteration for key-value tables. If order matters, use a table that contains only positional values, or sort the keys first.

#### Iterating Over an Empty Table
What happens if the table is empty?

```apex
empty = []
count = 0
for v in empty
    count = count + 1
```

The loop runs zero times. `count` stays 0. This is not an error — it's just a loop that doesn't execute. The same is true if the table has no values (like a key-value table with only keys but no values, which is unusual but possible).

#### The Loop Variable Holds a Reference
When you iterate over a table, the loop variable holds a **reference** to each value. For numbers and strings, this doesn't matter much — they're immutable anyway. But for tables, the loop variable points to the same table that's inside the container.

```apex
outer = [[1, 2], [3, 4]]
for inner in outer
    // inner is a reference to each inner table
    inner[1] = inner[1] * 10
```

After this, `outer` is `[[10, 2], [30, 4]]`. Because `inner` is a reference to the actual table inside `outer`, modifying `inner` modifies the original table.

If you wanted to modify a copy instead, you'd have to make the copy yourself. But usually, modifying in place is what you want.

#### You Cannot Iterate Over Strings
If you want to iterate over the characters of a string, `for value in string` won't work. Strings are not tables.

```apex
text = "hello"
for c in text     // ERROR — text is a string, not a table
    // ...
```

Apex requires the iterable to be a table. If you need to process characters one by one, you can split the string into a table of characters first, or use a counter loop over the string's length.

### For Condition
Sometimes you don't know in advance how many times you'll need to repeat something. You just know that you want to keep going as long as some condition is true. That's what the condition form of `for` is for.

Syntax:
```apex
for condition
    // code to run while condition is true
```

Notice there's no variable after `for`. Instead, you write a boolean expression — the same kind of condition you'd write in an `if` statement. Before each iteration, Apex checks the condition. If it's true, the body runs. If it's false, the loop stops.

Here's an example that counts from 1 to 5:

```apex
counter = 1
result = ""
for counter <= 5
    result = "{result}{counter}"
    counter = counter + 1
```

After this, `result` is `"12345"`. Let's trace through:
- `counter` is 1. The condition `counter <= 5` is true. The body runs: `result` becomes `"1"`, `counter` becomes 2.
- `counter` is 2. Condition true. `result` becomes `"12"`, `counter` becomes 3.
- … and so on …
- `counter` is 5. Condition true. `result` becomes `"12345"`, `counter` becomes 6.
- `counter` is 6. Condition `6 <= 5` is false. The loop stops.

Notice that `counter` is declared **before** the loop. The loop body modifies it. If you forgot to update `counter` inside the body, the condition would never change, and the loop would run forever. Apex won't stop you from writing an infinite loop — it will just keep running until you kill the program.

Always make sure something inside the loop body changes the condition. Common patterns are incrementing a counter, decrementing a counter, or reading new input each time.

#### The Condition Can Be Any Boolean Expression
The condition can be any boolean expression, including comparisons with `and`, `or`, and `not`. For example:

```apex
x = 1
y = 10
result = ""
for x < y and y > 5
    result = "{result}{x}"
    x = x + 2
    y = y - 1
```

This loop continues as long as `x < y` **and** `y > 5`. Each iteration, `x` increases by 2 and `y` decreases by 1. The condition is re-checked before each iteration.

#### The Condition Is Re-Evaluated Before Each Iteration
Unlike a counter loop, where the start and end values are captured once, the condition in a condition loop is re-evaluated before every iteration. That's the whole point — the condition can change based on what happens in the body.

```apex
i = 0
for i < 5
    i = i + 1
```

Before each run, `i < 5` is checked with the current value of `i`. When `i` becomes 5, the condition is false, and the loop stops.

This means you can write conditions that depend on the state of many variables, and the loop will adapt as those variables change. The loop keeps running as long as the condition is true, no matter how many iterations that takes.

#### Counting Down
Here's an example that counts down:

```apex
counter = 5
result = ""
for counter >= 1
    result = "{result}{counter}"
    counter = counter - 1
```

After this, `result` is `"54321"`. The loop runs while `counter >= 1`. Each iteration, `counter` decreases by 1. When `counter` reaches 0, the condition is false, and the loop stops.

#### A Warning About Infinite Loops
An infinite loop is a loop that never stops. It runs forever, or until the program is killed. This can happen accidentally if you forget to update the condition variable.

```apex
counter = 1
for counter <= 5
    result = "{result}{counter}"
    // forgot to increment counter!
```

Here, `counter` is always 1. The condition `1 <= 5` is always true. The loop runs forever, printing `"1"` each time. The program will hang until you interrupt it (usually with Ctrl+C).

Always double-check that your loop body changes the state that the condition depends on. If the condition doesn't change, the loop never stops.

#### The Condition Must Be Boolean
As with everything in Apex, the condition must be a boolean expression. You cannot write `for x` and expect it to mean "while x is not zero." You must write a comparison or a boolean variable.

```apex
x = 5
for x > 0          // OK — comparison
    // ...

y = true
for y == true      // OK — explicit boolean comparison
    // ...
```

If you write `for x` where `x` is a number, Apex will report an error. Always be explicit.

#### Zero Iterations
If the condition is false from the start, the loop never runs. This is not an error — it's just a loop with zero iterations.

```apex
x = 10
result = ""
for x < 5
    result = "{result}{x}"
```

Here, `x < 5` is false initially (10 is not less than 5). The loop body never runs. `result` stays `""`.

### Break
Sometimes you want to leave a loop early. Maybe you found what you were looking for and there's no point continuing. Or maybe an error occurred and you need to stop. The `break` statement exits the loop immediately.

When Apex sees `break`, it jumps out of the innermost loop, skipping any remaining iterations. Execution continues with the code after the loop.

Here's an example that searches for a value:

```apex
numbers = [10, 20, 30, 40, 50]
found = false
for n in numbers
    if n == 30
        found = true
        break
```

After this, `found` is `true`. The loop visits 10, then 20, then 30. When `n` is 30, the condition `n == 30` is true, so `found` becomes `true` and `break` runs. The loop stops immediately, and the remaining values (40, 50) are never visited.

Without `break`, the loop would continue to the end, but `found` would already be `true` — the extra iterations would just be wasted work. `break` saves time when you know there's nothing more to do.

#### Break in Counter Loops
You can use `break` with any form of `for`. Here's an example with a counter loop:

```apex
result = ""
for i = 1, 10
    if i == 5
        break
    result = "{result}{i}"
```

After this, `result` is `"1234"`. The loop runs `i` from 1 to 10, but when `i` reaches 5, `break` exits the loop. So the body only runs for `i` = 1, 2, 3, 4.

#### Break in Condition Loops
`break` is especially common with condition loops, where you're looping until something happens, and then you break when it does.

```apex
import os

for true == true       // infinite loop
    input = os.input("Enter a number (or 'quit'): ")
    if input == "quit"
        break
    n = number(input)
    if n == none
        os.output("Not a number")
    else
        os.output("Doubled: {n * 2}")
```

This loop runs forever, prompting the user for input. If the user types `"quit"`, `break` exits the loop. Otherwise, the input is processed and the loop repeats.

Using `for true == true` to write an infinite loop is a common pattern when you don't know how many iterations you'll need, but you know when to stop. Always make sure there's a `break` somewhere that will eventually be reached.

#### Break Only Exits the Innermost Loop
`break` only exits the **innermost** loop. If you have a loop inside another loop, `break` inside the inner loop exits only that inner loop. The outer loop continues.

```apex
result = ""
for i = 1, 3
    for j = 1, 5
        if j == 3
            break
        result = "{result}{i}{j} "
```

After this, `result` is `"111221223132 "`. Let's trace:
- `i = 1`: inner loop runs `j` from 1 to 5. When `j == 3`, break. So `j` runs 1, 2. `result` gets `"11 12 "`.
- `i = 2`: inner loop again runs `j` from 1, breaks at 3. `result` gets `"21 22 "`.
- `i = 3`: same thing. `result` gets `"31 32 "`.

The `break` only exits the inner loop. The outer loop continues. This is important — if you want to break out of both loops at once, you'd need some other mechanism (like a flag variable or a helper function with a `return`).

### Continue
Sometimes you don't want to exit the loop entirely — you just want to skip the rest of the current iteration and move on to the next one. That's what `continue` does.

When Apex sees `continue`, it stops executing the current iteration and jumps to the next one. The loop itself continues; only the current pass is cut short.

Here's an example that skips even numbers:

```apex
result = ""
for i = 1, 6
    if i % 2 == 0
        continue
    result = "{result}{i}"
```

After this, `result` is `"135"`. Let's trace through:
- `i` is 1. `1 % 2 == 0` is false, so we don't continue. `result` becomes `"1"`.
- `i` is 2. `2 % 2 == 0` is true, so `continue` runs. We skip the rest of the body — `result` is not changed.
- `i` is 3. Condition false. `result` becomes `"13"`.
- `i` is 4. Condition true. `continue`. Skip.
- `i` is 5. Condition false. `result` becomes `"135"`.
- `i` is 6. Condition true. `continue`. Skip.
- Loop ends.

The loop visited all six numbers, but the even ones were skipped. `continue` is useful when you want to ignore certain cases but still process the rest.

#### Continue in Table Iteration
`continue` works in table iteration loops too:

```apex
values = [1, 2, 3, 4, 5]
result = ""
for v in values
    if v % 2 == 0
        continue
    result = "{result}{v}"
```

After this, `result` is `"135"`. The loop skips the even numbers.

#### Continue Only Affects the Innermost Loop
Like `break`, `continue` affects only the innermost loop. In nested loops, `continue` skips to the next iteration of the inner loop, not the outer one.

```apex
result = ""
for i = 1, 2
    for j = 1, 3
        if j == 2
            continue
        result = "{result}{i}{j} "
```

After this, `result` is `"11132123 "`. Let's trace:
- `i = 1`:
  - `j = 1`: no continue. `result` gets `"11 "`.
  - `j = 2`: continue — skip. `result` not changed.
  - `j = 3`: no continue. `result` gets `"13 "`.
- `i = 2`:
  - `j = 1`: no continue. `result` gets `"21 "`.
  - `j = 2`: continue — skip.
  - `j = 3`: no continue. `result` gets `"23 "`.

The `continue` only skipped the inner loop's iterations. The outer loop ran all its iterations.

#### Continue vs. Break
These two statements look similar but do very different things:

| Statement | Effect |
|-----------|--------|
| `break` | Exits the loop entirely. |
| `continue` | Skips the rest of the current iteration and moves on to the next one. |

`break` says "I'm done with this loop." `continue` says "I'm done with this iteration."

Use `break` when you want to stop looping entirely. Use `continue` when you want to skip the current item but keep looping.

#### A Common Pattern: Filtering
A common use of `continue` is filtering — you want to process some items but skip others.

```apex
import os

numbers = [1, -2, 3, -4, 5]
total = 0
for n in numbers
    if n < 0
        continue
    total = total + n
os.output(total)   // 9 (1 + 3 + 5)
```

Here, the loop skips negative numbers and adds only the positive ones. The result is 9.

Without `continue`, you'd need an `if` that wraps the entire rest of the loop body:

```apex
for n in numbers
    if n >= 0
        total = total + n
```

This is also valid, but when the loop body is long, `continue` at the top is cleaner. It lets you say "skip negative numbers" early, and then the rest of the body doesn't need to be indented.

#### When to Use Continue
Use `continue` when:
- You want to skip the current item entirely.
- The skip condition is simple and can be checked early in the loop.
- The rest of the loop body would be deeply indented otherwise.

Don't overuse it. If your loop body has many `continue` statements, it might be a sign that you should restructure your logic. But used sparingly, `continue` keeps loops clean.

## Functions
### Why Functions Exist
Imagine you're writing a program that calculates the area of a circle. You write the formula once, and it works. Now imagine your program needs to calculate the area of ten different circles at ten different points. Would you write the same formula ten times? Of course not. That would be tedious, and if you ever needed to change the formula, you'd have to change it in ten places. Miss one, and your program is inconsistent.

This is the problem functions solve. A **function** is a named block of code that you can run whenever you want, as many times as you want, without writing it out again. You write the code once, give it a name, and then **call** that name whenever you need the code to run.

Think of a function like a recipe. A recipe has a name (like "Pancakes"), it might need ingredients (flour, milk, eggs), and it produces a result (a stack of pancakes). You don't rewrite the recipe every time you want pancakes. You just follow the recipe again. The recipe is the function. The ingredients are the **parameters**. The pancakes are the **return value**. And "making pancakes" is **calling** the function.

Functions give you three big benefits:

1. **Reusability.** Write once, use many times.
2. **Clarity.** A well-named function tells you what it does without you needing to read the code inside.
3. **Organization.** Complex programs become a collection of small, understandable pieces instead of one giant blob.

Every programming language has functions in some form. In Apex, they are simple, predictable, and pure. Let's learn how to write them.

### Function Statement
To create a function, you use the `function` keyword. Then you write the function's name. Then a pair of parentheses `()`. Then an indented block of code — the function body.

Syntax:
```apex
function name()
    // code that runs when the function is called
```

Here's a simple example:

```apex
import os

function say_hello()
    os.output("Hello!")
```

This defines a function called `say_hello`. The body contains one line: it prints `"Hello!"` to the terminal. Defining the function does not run it. It just tells Apex: "When I say `say_hello()`, run this code."

To actually run the code, you have to **call** the function. We'll cover calling in a moment. For now, just notice the shape: keyword `function`, then a name, then `()`, then an indented block.

#### Naming Functions
Function names follow the same rules as variable names. They can contain letters, digits, and underscores. They cannot start with a digit. They are case-sensitive: `say_hello` and `Say_Hello` are different names.

By convention, Apex uses `snake_case` for function names — all lowercase, with underscores between words. So `say_hello`, `calculate_area`, `find_user_by_id`. This makes names easy to read.

A good function name describes **what the function does**, not how it does it. `calculate_total` is better than `loop_and_add`. `is_valid` is better than `check_stuff`. When someone reads your code, the name should tell them what to expect.

#### Functions Are Values
When you define a function, Apex stores it as a **function value** — just like numbers, strings, and tables. You can assign it to a variable, pass it around, and store it in tables. But for now, we'll keep it simple and focus on the basics.

### Parameters
A function that always does the same thing is useful, but limited. Most functions need **input** — information to work with. That's what parameters are for.

A parameter is a named slot that the function expects to receive when it's called. You list parameters inside the parentheses, separated by commas.

Syntax:
```apex
function name(param1, param2, param3)
    // code can use param1, param2, and param3
```

Here's a function with one parameter:

```apex
import os

function greet(name)
    os.output("Hello, {name}!")
```

This function is called `greet`. It takes one parameter called `name`. Inside the body, `name` is used in a string interpolation. When someone calls `greet("Alice")`, the parameter `name` becomes `"Alice"`, and the function prints `"Hello, Alice!"`.

The parameter `name` is a variable. It exists only inside the function. It's created when the function is called, and it disappears when the function finishes. You can use it anywhere inside the body, just like any other variable.

Here's a function with two parameters:

```apex
import os

function add(a, b)
    result = a + b
    os.output("{a} + {b} = {result}")
```

When you call `add(5, 3)`, the parameter `a` becomes `5`, `b` becomes `3`, and the function prints `"5 + 3 = 8"`.

The order matters. The first value you pass goes into the first parameter, the second value goes into the second parameter, and so on. So `add(5, 3)` and `add(3, 5)` both work, but they set the parameters differently.

#### Parameters Are Local
A parameter is just a local variable. It exists inside the function and nowhere else. If you have a variable with the same name outside the function, they are different variables. The parameter shadows the outer one inside the function body.

For example:

```apex
import os

name = "Outer"

function greet(name)
    os.output("Hello, {name}!")

greet("Alice")
os.output("Outside: {name}")
```

This prints:
```text
Hello, Alice!
Outside: Outer
```

Inside `greet`, the parameter `name` is `"Alice"`. Outside, the variable `name` is still `"Outer"`. They don't interfere.

#### Parameters Must Be Provided
Apex does not have default parameter values. If a function declares two parameters, you must call it with exactly two arguments. Not one. Not three. Exactly two.

If you try to call `add(5)` when `add` expects two parameters, Apex will report an error. If you call `add(5, 3, 1)`, Apex will also report an error. This strictness is deliberate: functions should be predictable, and part of predictability is knowing exactly what input they expect.

#### Parameters Are Copies
When you pass a value to a function, the parameter receives a **copy** of that value. If you change the parameter inside the function, the original value outside is not affected.

For numbers, strings, booleans, and none, this is straightforward. They are **immutable** — you can't change them anyway. You can only reassign the variable to point to a new value.

For tables, the story is different. A table is a **reference type**. When you pass a table to a function, the parameter points to the same table. If you modify the table inside the function — by setting a key or appending an item — those changes are visible outside. But if you reassign the parameter to a completely new table, the outer variable still points to the original.

This distinction is important, but it's a subtle one. For now, just remember: numbers, strings, booleans, and none are copied. Tables are shared. We'll revisit this when we talk about tables more deeply.

### Return Value
A function can do work, but often you want it to **give you back a result**. That's what return values are for. When a function returns a value, the call to that function **evaluates** to that value. You can assign it to a variable, use it in an expression, or pass it to another function.

To return a value, use the `return` keyword followed by an expression:

```apex
function add(a, b)
    return a + b
```

This function takes two parameters and returns their sum. When you call `add(5, 3)`, the function runs, computes `5 + 3`, and returns `8`. The call `add(5, 3)` becomes `8` — as if you'd written the number `8` directly.

You can use the return value like this:

```apex
import os

function add(a, b)
    return a + b

result = add(5, 3)
os.output(result)  // prints 8
```

Here, `result` receives the value `8`, which came from the function. Then `os.output` prints it.

You can also use the return value directly:

```apex
os.output(add(10, 20))  // prints 30
```

The function returns `30`, and `os.output` prints it.

#### Returning Early
The `return` statement does two things: it gives a value back to the caller, and it **immediately exits the function**. Nothing after `return` runs.

```apex
function check_positive(n)
    if n > 0
        return "positive"
    return "not positive"
```

If `n` is `5`, the condition `n > 0` is true, so `return "positive"` runs, and the function exits immediately. The final `return "not positive"` is never reached.

If `n` is `-3`, the condition is false, so the first `return` is skipped. The function continues to `return "not positive"`, which runs and exits the function.

This pattern — checking a condition and returning early — is very common. It keeps your code flat and easy to read, avoiding deeply nested `if` statements.

#### Functions Without a Return
Not every function needs to return a value. Some functions just do something — print a message, write a file, modify a table. If a function has no `return`, it returns `none` automatically when it reaches the end.

```apex
import os

function say_hello()
    os.output("Hello!")

result = say_hello()
os.output(result)  // prints none
```

Here, `say_hello` prints `"Hello!"`, then reaches the end of the function. Since there's no `return`, it returns `none`. The variable `result` gets `none`.

You can also write `return none` explicitly if you want to be clear that the function returns nothing meaningful. But it's not required.

#### Return Ends the Function
Once `return` runs, the function is done. Nothing after it runs, not even if there's more code in the body.

```apex
function example()
    return 42
    os.output("This never runs")  // unreachable
```

Apex will actually warn you that the line after `return` is unreachable. It's dead code, and dead code is usually a mistake.

#### A Function Returns Exactly One Value
Apex functions return exactly one value. That value can be a number, a string, a boolean, none, or a table. But it's always exactly one thing.

You cannot write `return a, b` to return two values. If you need to return multiple pieces of information, you can put them in a table and return the table.

```apex
function min_max(numbers)
    // ... compute minimum and maximum ...
    return ["min" = min_value, "max" = max_value]
```

Then the caller receives a table and can access its parts. This is the idiomatic way to return multiple values in Apex.

### Call
Defining a function doesn't run it. To run it, you **call** it. Calling a function means writing its name followed by parentheses, with any arguments inside the parentheses if the function expects parameters.

Syntax:
```apex
name(arg1, arg2, ...)
```

If the function has no parameters, the parentheses are empty:

```apex
say_hello()
```

If the function has parameters, you list the values inside:

```apex
greet("Alice")
add(5, 3)
```

The values you pass are called **arguments**. The names inside the function definition are called **parameters**. They're often used interchangeably, but the distinction is useful: parameters are the slots, arguments are the values you put in them.

When Apex sees a function call, it:
1. Evaluates each argument to get its value.
2. Creates a new scope for the function.
3. Binds each parameter to the corresponding argument value.
4. Runs the function body.
5. If a `return` is reached, that value becomes the result of the call.
6. If the end of the body is reached without a `return`, the result is `none`.
7. Destroys the function's scope, including all parameters and local variables.
8. The call expression evaluates to the returned value.

You can use a function call anywhere you can use a value. That means you can:

- Assign it to a variable: `result = add(5, 3)`
- Use it in an expression: `total = add(5, 3) * 2`
- Pass it to another function: `os.output(add(5, 3))`
- Use it in a condition: `if is_valid(x) == true`

And because functions can call other functions, you can build up complex behavior from simple pieces.

### Scope and Blocks
Every function creates a new **scope**. A scope is a region of code where a variable exists. Variables declared inside a function — including its parameters — are **local** to that function. They are created when the function is called, and they are destroyed when the function returns.

This means:

1. You cannot access a function's local variables from outside.
2. Two different functions can use the same variable name without conflict.
3. A function can read variables from outer scopes, but it cannot assign to them in a way that affects the outer scope (for numbers, strings, booleans, and none).

Let's look at an example:

```apex
import os

x = "outer"

function test()
    y = "inner"
    os.output(x)  // reads outer variable
    os.output(y)  // reads local variable

test()
os.output(y)  // ERROR: y is not defined here
```

Inside `test`, the variable `x` is visible because it was declared outside. But `y` is local to `test`. After `test` returns, `y` is gone. Trying to use it outside causes an error.

The function can read `x`, but it cannot change `x` in a way that affects the outside. If it assigns to `x`, it creates a new local variable that shadows the outer one.

```apex
import os

x = "outer"

function test()
    x = "inner"  // creates a new local x
    os.output(x)  // prints "inner"

test()
os.output(x)  // prints "outer"
```

Inside `test`, `x = "inner"` creates a new local variable `x`. The outer `x` is untouched. When `test` returns, the local `x` disappears, and the outer `x` is still `"outer"`.

#### Functions Inside Functions
You can define a function inside another function. The inner function can read the outer function's variables, but the outer function cannot read the inner function's variables.

```apex
import os

function outer()
    message = "Hello from outer"
    
    function inner()
        os.output(message)  // reads outer's message
    
    inner()

outer()
```

Here, `inner` is defined inside `outer`. It can read `message` because `message` is in an enclosing scope. When `outer` calls `inner`, the message is printed.

Nested functions are useful for organization, but they should be used sparingly. Most of the time, a flat structure with well-named functions at the top level is clearer.

#### Blocks Inside Functions
You already know that `if` and `for` create their own blocks and scopes. The same is true inside functions. A variable declared inside an `if` block is local to that block. It does not exist after the block.

```apex
function example()
    if true == true
        temp = "inside if"
        // temp exists here
    // temp does not exist here
```

This rule is consistent throughout Apex: **indentation defines scope**. Wherever you indent, you create a new scope. Variables live and die within their scope.

### Early Return
The `return` statement can appear anywhere in a function, not just at the end. When it runs, the function exits immediately, and no further code in that function runs.

This is called **early return**, and it's a powerful way to keep your functions readable.

Consider a function that validates a username:

```apex
function is_valid_username(name)
    if name == none
        return false
    if string.length(name) < 3
        return false
    if string.length(name) > 20
        return false
    return true
```

This function checks several conditions. If any of them fail, it returns `false` immediately. Only if all conditions pass does it return `true`. The logic is flat and easy to follow.

Without early return, you'd need to nest everything:

```apex
function is_valid_username(name)
    if name != none
        if string.length(name) >= 3
            if string.length(name) <= 20
                return true
            else
                return false
        else
            return false
    else
        return false
```

This is harder to read. The nesting obscures the logic. Early return flattens it out.

Use early return when:
- You're validating input and want to bail out on the first problem.
- You've found what you're looking for and don't need to continue.
- You're handling error cases and want to get them out of the way.

Functions can have multiple `return` statements, but only one of them will actually run on any given call. The first one reached is the one that exits.

### Recursion
A function can call itself. This is called **recursion**, and it's a powerful technique for solving problems that have a naturally repetitive structure.

The classic example is factorial. The factorial of a number `n` is the product of all positive integers from 1 to `n`. For example, `5! = 5 × 4 × 3 × 2 × 1 = 120`.

You can define factorial recursively: `n! = n × (n-1)!`, with the base case `0! = 1`.

```apex
function factorial(n)
    if n <= 1
        return 1
    return n * factorial(n - 1)
```

Let's trace `factorial(4)`:
- `n` is 4. Not `<= 1`. So return `4 * factorial(3)`.
- `n` is 3. Not `<= 1`. So return `3 * factorial(2)`.
- `n` is 2. Not `<= 1`. So return `2 * factorial(1)`.
- `n` is 1. `1 <= 1` is true. Return `1`.
- So `factorial(2)` returns `2 * 1 = 2`.
- `factorial(3)` returns `3 * 2 = 6`.
- `factorial(4)` returns `4 * 6 = 24`.

Recursion works because each call to `factorial` creates a new scope with its own `n`. They don't interfere with each other. The calls stack up until the base case is reached, then the results unwind back.

Every recursive function needs a **base case** — a condition where it returns without calling itself. Without a base case, the function would call itself forever, and Apex would eventually report a stack overflow.

Apex has a maximum call depth of 1024 frames. If your recursion goes deeper than that, the program stops with an error. Most recursive algorithms stay well under this limit, but deeply recursive ones might need to be rewritten as loops.

## Async / Await
### Why Async Exists
Imagine you're writing a program that needs to read a large file from disk. Reading a file is not instant. It takes time — maybe a few milliseconds, maybe a few seconds. While the computer is fetching that data, what should your program do? Should it freeze and wait, doing nothing until the file is ready? Or should it keep doing other useful work in the meantime?

In most simple programs, the answer is "just wait." You ask for the file, and your program pauses until the file arrives. This is called **blocking**. It's simple and predictable. For small scripts, it's perfectly fine.

But imagine your program has more to do. Maybe it needs to read ten files, or wait for a network response, or sleep for a second between steps. If each of those operations blocks the whole program, you're wasting time. While waiting for one thing, you can't do anything else.

This is where **async** and **await** come in. They let you say: "Start this slow operation, but don't stop the whole program while you wait. Let me do other things. When the result is ready, I'll come back for it."

The idea is simple, but the mechanics take a moment to get used to. Let's build up from the ground.

### What Is a Future
A **future** is a value that represents a result you don't have yet — but will have later. It's like a claim ticket at a coat check. You hand over your coat, and you get a ticket. The ticket isn't the coat. It's a promise that says: "Your coat will be ready when you come back with this ticket."

You can hold onto the ticket, put it in your pocket, or hand it to a friend. You don't have to wait at the counter. You can go do other things. When you're ready to get your coat, you show the ticket, and if the coat is ready, you receive it. If it's not ready yet, you wait a little longer.

In Apex, a future is a real value — just like a number, string, or table. You can store it in a variable, put it in a table, pass it to a function. It represents a result that will be available at some point.

You create a future by calling an **async function**. Calling a normal function runs its body immediately and gives you the result. Calling an async function does something different: it **starts** the function's body in the background, and immediately returns a future. You get the ticket right away. The result comes later.

### Async Function
To create an async function, you put the word `async` before the word `function`.

Syntax:
```apex
async function name(params)
    // body
```

Everything else stays the same. You still list parameters, you still use `return`, you still call it with parentheses. The only difference is that calling it does not run the body to completion and give you the return value directly. Instead, it returns a **future**.

Here's a simple async function:

```apex
async function add(a, b)
    return a + b
```

This function adds two numbers and returns the sum. The body is trivial. But because it's marked `async`, calling `add(2, 3)` does not give you `5`. It gives you a **future** that will eventually hold `5`.

```apex
future = add(2, 3)
// future is a future, not 5
```

To get the actual value out of a future, you need `await`. We'll get there in a moment.

#### Why Mark a Function Async?
For a function as simple as `add`, there's no reason to make it async. The body is instantaneous. The whole point of async is to run slow work in the background without blocking. So async functions usually do things like:

- Read a file with `os.read`.
- Wait for a timer with `os.wait`.
- Perform a network request.
- Run a long computation.

Here's an example of an async function that waits:

```apex
import os

async function delayed_greeting(name)
    await os.wait(1)  // wait one second in the background
    return "Hello, {name}!"
```

When you call `delayed_greeting("Alice")`, the function starts running in the background. It immediately hits `await os.wait(1)`, which schedules a one-second timer and suspends the function. The caller gets a future back right away. After a second passes, the function resumes, builds the greeting string, and the future resolves to `"Hello, Alice!"`.

The caller was never blocked. If the caller had other work to do, it could do that work during the one-second wait.

#### Async Functions Return Futures
Every async function, no matter how simple, returns a future. Even `async function add(a, b) return a + b` returns a future, not a number.

This is the crucial rule to internalize: **calling an async function gives you a future, not the result**. The result arrives later, and you retrieve it with `await`.

### Await
The word `await` means: "I want the result of this future. If it's ready, give it to me now. If it's not ready yet, wait until it is."

You use `await` before a call to an async function. The call is what produces the future; `await` unwraps it.

Syntax:
```apex
result = await async_function(args)
```

The `await` keyword tells Apex: "Call this async function, start its body in the background if it hasn't already started, and give me the value when it's ready."

Here's a complete example:

```apex
import os

async function add(a, b)
    return a + b

async function main()
    result = await add(2, 3)
    os.output(result)  // prints 5

await main()
```

Let's trace through this:
- `main` is called with `await`. It's async, so its body starts running.
- Inside `main`, `add(2, 3)` is called with `await`. This starts `add`'s body in the background.
- `add` returns `5` almost instantly. `await` receives `5` and assigns it to `result`.
- `os.output(result)` prints `5`.
- `main` finishes, and the top-level `await main()` completes.

The `await` in front of `add(2, 3)` is what turns the future into the actual number.

#### Await Does Not Block Everything
The word "wait" might make you think `await` freezes the whole program. It doesn't. When `await` encounters a future that isn't ready yet, it **suspends the current function** and lets the rest of the program continue. Other functions, other coroutines, and the scheduler keep running. Only the current function pauses.

Think of it like this: you're in a restaurant, and you've ordered food. Instead of standing at the counter staring at the kitchen, you go back to your table and chat with friends. When the waiter brings your food, you eat. You didn't block anyone; you just paused your own waiting until the food arrived.

`await` works the same way. It pauses the function that called it, but the rest of the program keeps going. When the future resolves, the function resumes right where it left off.

#### Await Always Gives a Value
The result of `await` is the resolved value of the future. If the async function returned `5`, `await` gives you `5`. If it returned a string, you get the string. If it returned a table, you get the table. If it returned `none`, you get `none`.

You can use that value like any other:

```apex
async function main()
    x = await get_number()
    y = await get_number()
    sum = x + y
    os.output("Sum: {sum}")
```

Each `await` retrieves one value. The values are ordinary Apex values.

### Where Await Can Be Used
`await` has strict rules about where it can appear. It can only be used in two places:

1. **Inside an async function.**
2. **At the top level of the program.**

That's it. You cannot use `await` inside a normal (non-async) function. You cannot use it inside an `if` at the top level unless that `if` is itself at the top level.

The reason is that `await` requires the scheduler to be able to pause and resume the surrounding function. Only async functions (and the top-level program) are set up to support that. A regular function must run from start to finish without interruption.

If you try to use `await` inside a normal function, Apex will report an error: `'await' outside of async function`.

#### Top-Level Await
At the top level of your program — that is, in the code that runs immediately when the program starts — you can use `await` directly. This is convenient for small scripts that need to await one or two things.

```apex
import os

async function fetch_name()
    await os.wait(0.5)
    return "Alice"

name = await fetch_name()
os.output(name)  // prints "Alice" after a half-second pause
```

Here, `await fetch_name()` is at the top level. It's allowed. The program runs `fetch_name` in the background, waits for it to finish, gets `"Alice"`, and prints it.

#### Await Inside Async Functions
You can also use `await` inside any async function. This is how you compose async operations: one async function awaits another, which awaits another, and so on.

```apex
import os

async function read_two_files(path1, path2)
    content1 = await os.read(path1)
    content2 = await os.read(path2)
    return content1 + content2
```

Here, `read_two_files` awaits `os.read` twice. Each call runs in the background. The function suspends while waiting, then resumes when each file is ready.

You can also await user-defined async functions:

```apex
async function outer()
    result = await inner()
    return result * 2

async function inner()
    return 42

final = await outer()  // 84
```

The nesting can go as deep as you need. Each level of `await` unwraps one layer of future.

### What Can Be Awaited
Not everything can be awaited. The operand of `await` must be either:

1. **A call to an async function.**
2. **A call to a builtin that supports async.**

You cannot await a number. You cannot await a string. You cannot await a normal function call. You cannot await a variable that holds a future — you can only await the **call** that produces the future.

Wait — that last one needs clarification. The rule is that `await` must be followed by a function call. You write:

```apex
result = await async_function()
```

You cannot write:

```apex
fut = async_function()
result = await fut  // ERROR: 'await' requires a call
```

The future stored in `fut` is real, but Apex requires the await to see the call directly. This is a design choice that keeps the scheduler simple and predictable. If you need to store an async call's future for later, you would typically structure your code so the await happens immediately, or you would restructure so the async function does the work internally.

In practice, this restriction is rarely a problem. You usually call an async function and await it right away.

#### Awaiting Builtins
Many built-in functions in Apex support the async protocol. When you put `await` before a call to one of these builtins, the work is offloaded to a background worker thread, and the caller continues without blocking. The future resolves when the worker is done.

Builtins that can be awaited include:

- `os.read` — reading a file.
- `os.write` — writing a file.
- `os.append` — appending to a file.
- `os.execute` — running a shell command.
- `os.wait` — sleeping for a duration.
- `os.copy`, `os.move`, `os.rename`, `os.delete`, `os.create_file`, `os.create_folder`, `os.list_folder`, `os.size`, `os.exists`, `os.is_file`, `os.is_folder`, `os.parent_folder`, `os.access`, `os.terminate` — various filesystem and process operations.
- `json.decode`, `json.encode` — JSON processing.
- `xml.decode`, `xml.encode` — XML processing.
- `csv.decode`, `csv.encode` — CSV processing.
- `base.encode_*`, `base.decode_*` — base encoding.
- `regex.find_all`, `regex.replace`, `regex.split`, `regex.search` — regex operations.
- `zip.pack`, `zip.unpack` — ZIP archives.

When you call these with `await`, the operation runs off the main thread. The current function suspends. When the operation completes, the function resumes with the result.

```apex
import os
import json

async function load_config(path)
    text = await os.read(path)
    if text == none
        return none
    return json.decode(text)

config = await load_config("config.json")
```

Here, `await os.read(path)` reads the file in the background. Then `await load_config(...)` (at the top level) runs the whole function as a background coroutine. The result is the parsed config or `none`.

#### Awaiting Without Awaiting
It's worth noting: if you call one of these builtins without `await`, it runs synchronously — that is, it blocks the current thread until it's done.

```apex
text = os.read(path)  // blocks until the file is read
```

This is fine for simple scripts. The async versions are for when you want to keep the program responsive while doing slow work.

#### When You Don't Need to Await
You don't have to await everything. If you're at the top level and you just need a value, you can call async functions and use the result... but wait, that's not true. Calling an async function gives you a future, not the value. So you do need to await.

The exceptions are the synchronous builtins — the ones you call without `await`. Those are fine.

The rule is simple: **if it's an async function, you must await it to get its value.** If it's a normal function or a synchronous builtin, you get the value directly.

### Running in the Background
When you write `await some_function()`, the body of `some_function` starts running in the background. This is important: it's not "run later" or "queue for some future time." It starts now.

What happens next depends on whether `some_function` reaches an `await` of its own:

- If `some_function` runs to completion without ever suspending, its future resolves almost immediately. The `await` in the caller retrieves the value right away.
- If `some_function` suspends (because it awaits something), the caller's `await` also suspends. The scheduler pauses both functions and continues with other work. When the inner operation completes, `some_function` resumes, and eventually its future resolves, waking up the caller.

This means that awaiting an async function that internally awaits other things chains the suspensions together. The scheduler manages the whole chain, resuming functions in the right order.

#### Example: A Chain of Awaits
```apex
import os

async function level_three()
    await os.wait(0.1)
    return "three"

async function level_two()
    result = await level_three()
    return "two + {result}"

async function level_one()
    result = await level_two()
    return "one + {result}"

final = await level_one()
os.output(final)  // prints "one + two + three"
```

Let's trace:
- Top-level `await level_one()` starts `level_one` in the background.
- `level_one` calls `await level_two()`, which starts `level_two` in the background.
- `level_two` calls `await level_three()`, which starts `level_three` in the background.
- `level_three` hits `await os.wait(0.1)`, which schedules a 0.1-second timer and suspends.
- All four functions — top-level, `level_one`, `level_two`, `level_three` — are now suspended.
- After 0.1 seconds, the timer fires. `level_three` resumes, returns `"three"`.
- `level_two` resumes with `"three"`, builds `"two + three"`, returns it.
- `level_one` resumes with `"two + three"`, builds `"one + two + three"`, returns it.
- Top-level resumes with the final string, prints it.

All of this happens without blocking the program. If there were other coroutines running, they would have continued during the 0.1-second pause.

#### The Scheduler
Under the hood, Apex has a **scheduler**. The scheduler is the part of the runtime that manages all the suspended functions and decides which one runs next. You don't interact with the scheduler directly. It works invisibly when you use `await`.

Every time a function suspends, the scheduler keeps track of where it was and what it's waiting for. When the awaited operation finishes, the scheduler resumes the function.

You don't need to know the details of the scheduler to use `await`. But it's good to know it exists, because it explains why `await` doesn't block everything and why some functions can pause and resume.

## Imports
### Why Imports Exist
So far, every program you've written has lived in a single file. That's fine for small scripts — a hundred lines, maybe a few hundred. But real programs grow. They get bigger. A task tracker might have functions for storing tasks, functions for displaying them, functions for formatting dates, functions for reading and writing files, and hundreds of lines of logic connecting everything together.

If you put all of that in one file, the file becomes a maze. You scroll forever to find the function you need. You lose track of what belongs with what. You can't hand a piece of the program to a teammate without also handing them everything else.

The solution is to split your code into multiple files. Each file holds a related group of functions and variables — a **module**. One file might handle math utilities, another might handle string formatting, another might hold configuration. You work on each file separately, keeping it small and focused.

But files can't be completely isolated. Sometimes the math file needs a helper from the string file. Sometimes the main program needs to call functions from both. That's what **imports** are for. An import tells Apex: "This file needs to use things from that other file. Go load it and make its contents available."

Think of imports like borrowing tools from a friend's workshop. Instead of buying your own drill, you go next door and say "I need your drill for this job." The drill lives in your friend's workshop, but you can use it in yours. That's an import.

In Apex, imports are simple. There's one keyword: `import`. You write it, name the file or library you want, and Apex handles the rest.

### Importing an Entire File
The most basic form of import brings in an entire file. You use the `import` keyword, followed by the file path.

Syntax for a file in the same folder:
```apex
import database.apex
```

This tells Apex: "Load the file `database.apex` from the same folder as this one, and make everything it defines available to me."

Wait — but a moment ago I said a file is a module. When you import a file, you don't get its contents dumped into your current scope with no structure. You get access to them **under the file's name**. So if `database.apex` defines a function called `connect`, you would call it like this:

```apex
import database.apex

database.connect()
```

The prefix `database.` tells Apex: "Look in the database module for the `connect` function." This is how you know, when you're reading code, where each function comes from. If you see `string.length(...)`, you know it's from the string module. If you see `database.connect()`, you know it's from your database file.

Here's a concrete example. Suppose you have a file called `math_utils.apex`:

```apex
// math_utils.apex

function square(x)
    return x * x

function cube(x)
    return x * x * x

pi = 3.14159
```

And a main file called `main.apex` in the same folder:

```apex
// main.apex

import math_utils.apex

import os

result = math_utils.square(5)
os.output(result)  // prints 25

area = math_utils.pi * math_utils.square(3)
os.output(area)    // prints 28.27431
```

Notice how the imported functions are called with the module prefix: `math_utils.square(5)`. The variable `pi` from `math_utils` is also accessed through the prefix: `math_utils.pi`.

This prefixing is deliberate. Without it, imagine if two files both define a function called `square` — one for numbers and one for matrices. If imports dumped everything into one flat namespace, one would overwrite the other, and calling `square` would be ambiguous. With prefixes, `math_utils.square` and `matrix_utils.square` are distinct. The prefix is the file's name, and it's how Apex keeps things organized.

Notice that we also imported `os` in the example. `os` is a built-in module — we'll cover that distinction later. For now, focus on the file import.

#### The `.apex` Extension Is Required
For user files, you must include the `.apex` extension in the import. If you write `import math_utils` without the extension, Apex won't know whether you mean a file, a folder, or something else. So always write `import math_utils.apex`.

Actually, let me correct that — the rule is: the import path must end with `.apex`. If you write `import math_utils`, Apex will report an error saying the path must end with `.apex`. This is a hard rule. Built-in modules (like `os`, `math`, `json`) are the exception, because they don't correspond to files on disk; they're part of the interpreter itself. We'll discuss that more in a bit.

#### Where Does Apex Look for the File?
All user file imports are resolved relative to the **main file** — the file you actually ran with `apex main.apex`. Not the file that contains the import, but the main file. This is important because it makes all your imports consistent. No matter how deep in your folder structure a file is, when it imports something, that import path is written as if it were being written from the main file's folder.

Let's look at an example of why this matters. Suppose your project looks like this:

```
my_project/
├── main.apex
├── database.apex
└── utils/
    └── string_utils.apex
```

If `main.apex` wants to use `string_utils.apex`, it writes:

```apex
import utils/string_utils.apex
```

Now suppose `database.apex` also wants to use `string_utils.apex`. It writes the same thing:

```apex
import utils/string_utils.apex
```

Even though `database.apex` is in the project root, and `string_utils.apex` is in `utils/`, the import path is still `utils/string_utils.apex` because it's written as if from the main file. This is deliberate — it keeps imports consistent. You never have to think about "where is this file relative to this other file?" You just write it once from the main file's perspective, and it works everywhere.

We'll come back to this rule when we talk about sub-folders.

### What Gets Imported
When you import a file, what exactly do you get access to? This is a question you might be asking.

The answer: **all globals** defined in the imported file. That means:

- **Functions.** Every `function` declared at the top level of the imported file.
- **Variables.** Every variable declared at the top level.
- **Constants.** Every `constant` declared at the top level.

What does **not** get imported:

- Local variables inside functions. Those are private to their functions and always will be.
- Anything that's inside a nested scope (like a variable declared inside an `if` block at the top level). Only the top-level definitions are shared.

Here's an example to make this clear. Suppose `helpers.apex` looks like this:

```apex
// helpers.apex

greeting = "Hello"

function greet(name)
    return "{greeting}, {name}!"

function mystery()
    secret = "I am local"
    return secret
```

When you import `helpers.apex`, you get access to `helpers.greeting` and `helpers.greet`. You do **not** get access to `secret`, because it's inside the `mystery` function.

You also get access to `helpers.mystery`, because `mystery` itself is a top-level function. Its body is not visible, but the function is.

#### Modules Run Once
A subtle but important point: when Apex imports a file, the top-level code in that file runs exactly once. Even if two different files import the same module, the module's body runs a single time. This is important because it means any initialization code (like setting up a global variable based on the environment) only runs once, no matter how many places use the module.

```apex
// config.apex

import os
import sys

host = sys.host()
started_at = datetime.timestamp()
```

If `config.apex` is imported from three different files, `sys.host()` and `datetime.timestamp()` are only called once. The module remembers its state from that single execution.

This is good news for both performance and consistency. You don't get three different values for `started_at`. You get the same value everywhere, because the module ran once.

### Importing from Sub-folders
Big projects rarely keep all files in one folder. They organize them into sub-folders — one for utilities, one for network code, one for tests. Apex supports this with a simple rule: use forward slashes `/` in your import paths to walk into folders.

Suppose your project looks like this:

```
my_project/
├── main.apex
└── utils/
    ├── math.apex
    └── string.apex
```

To import `math.apex`, you write:

```apex
import utils/math.apex
```

The `/` in `utils/math.apex` tells Apex: "Look in the folder `utils`, then in the file `math.apex`." You can nest deeper, too:

```
my_project/
├── main.apex
└── utils/
    └── internal/
        └── helpers.apex
```

```apex
import utils/internal/helpers.apex
```

Each `/` represents one level of descent.

You can use as many levels as you want, but if you go more than a few levels deep, consider whether your project structure is trying to tell you something. Deep folder structures can be hard to navigate. Most projects stay within two or three levels.

Once imported, you access things using the module name — which is the file's name without the `.apex` extension, not including the folders. So `utils/math.apex` becomes just `math` when used:

```apex
import utils/math.apex

result = math.square(5)
```

Notice that we write `math.square(5)`, not `utils.math.square(5)`. The folder structure is part of the import path, but the module name is just the file name.

#### A Caution About Names
Because the module name is the file name, two files with the same name in different folders will both be called the same thing inside your program. For example, if you have both `utils/math.apex` and `helpers/math.apex`, importing both gives you two modules both named `math`. This is confusing and probably a bug in your project structure.

Apex won't stop you from doing this, but you should avoid it. Pick unique file names for your modules. If you must have two files with the same name — for example, a `math.apex` for testing and a `math.apex` for production — use aliasing to give one of them a different name. We'll cover aliasing shortly.

### Importing from One Sub-folder into Another
Here's the tricky case. You have two files, each in its own sub-folder. One file wants to use something from the other. How do you write the import?

Remember the rule: **every import path is written relative to the main file**. Not relative to the importing file, not relative to the current working directory, but relative to the main file.

Let's look at the example structure from the Apex Express Course:

```
my_project/
├── main.apex
├── helpers/
│   └── math.apex
└── features/
    └── calculator.apex
```

Now, `calculator.apex` wants to use the `power` function from `helpers/math.apex`. How does it write the import? Even though `calculator.apex` is inside `features/`, and `math.apex` is inside `helpers/`, and both are siblings under `my_project/`, you do **not** write something like `../helpers/math.apex`. Apex does not support `..` paths.

Instead, you write:

```apex
import helpers/math.apex
```

That's it. This looks like `calculator.apex` is at the project root, not inside `features/`. And that's exactly the point: **all imports are written as if from the main file's location**. Since `main.apex` is at the project root, it would import `math.apex` as `helpers/math.apex`. So `calculator.apex` does the same thing, even though it's not at the project root.

This rule is unusual at first — many other languages use relative paths like `../`. But once you get used to it, it's actually simpler. You never have to count how many `..` you need. Every path is written the same way. Every import in every file reads like it was written from the top of your project.

Let's trace through a complete example to make sure it's clear.

**File: `my_project/main.apex`**

```apex
import features/calculator.apex

result = calculator.add(5, 3)
os.output(result)
```

**File: `my_project/features/calculator.apex`**

```apex
import helpers/math.apex

function add(a, b)
    return a + b

function power_add(a, b, exp)
    return math.power(a, exp) + b
```

**File: `my_project/helpers/math.apex`**

```apex
function power(base, exp)
    result = 1
    for i = 1, exp
        result = result * base
    return result
```

Here's what happens:
1. Apex runs `main.apex`. It sees `import features/calculator.apex`.
2. It loads `calculator.apex`. Inside, there's `import helpers/math.apex`.
3. Apex loads `helpers/math.apex` relative to the main file (`my_project/`), finding it at `my_project/helpers/math.apex`.
4. Everything resolves, and `main.apex` can call `calculator.add(5, 3)`.

The import path `helpers/math.apex` inside `calculator.apex` is written exactly as `main.apex` would have written it. Both files use the same path. This consistency is the whole point of the rule.

### Aliasing
Sometimes a module name is long, or awkward, or you just want a shorter name to type. Apex lets you give a module an **alias** — a shorter name you can use instead.

Syntax:
```apex
import utils/calculator.apex as calc
```

Now, everywhere in your file, you can refer to the module as `calc` instead of `calculator`:

```apex
result = calc.add(2, 3)
calc.print_result(result)
```

The `as` keyword introduces the alias. It comes after the file path, and the alias must be a valid name — letters, digits, and underscores, not starting with a digit.

Aliases are purely a convenience. They change nothing about the module itself. They just give you a shorter way to refer to it. Use them when the original name is verbose, or when it clashes with something else.

#### Aliases Must Be Unique
Each alias in a single file must be different from every other alias and from every other module name. If you import two modules and give them the same alias, Apex will report an error.

```apex
import utils/calculator.apex as calc
import utils/statistics.apex as calc  // ERROR: alias 'calc' already used
```

This is enforced because a duplicate alias would make it ambiguous which module you meant. Always choose aliases that don't clash.

#### Aliases Are Only for User Modules
Here's a rule that trips people up: you **cannot** alias a built-in module. If you try to write `import os as system`, Apex will report an error: "Cannot use 'as' alias with built-in module 'os'."

Why? Because built-in modules have canonical names that every Apex programmer knows. `os` is always `os`. `math` is always `math`. If you could alias them, code that uses `os.output` in one file might use `system.output` in another, and suddenly reading code becomes guesswork. Apex keeps built-in names fixed so that anyone reading your code knows exactly where each built-in function comes from.

User modules, on the other hand, are yours. You can name them whatever makes sense in your project, and you can alias them to whatever is convenient. So aliasing exists only for user modules.

### Built-in Modules Are Different
We've been talking about importing files — files that live on disk, written by you or your teammates. But Apex also ships with a set of **built-in modules**: `os`, `sys`, `math`, `string`, `table`, `random`, `json`, `xml`, `csv`, `base`, `regex`, `crypto`, `zip`, `datetime`.

These are not files. They're part of the interpreter itself, written in C. They're always available. But — and this is the catch — they still require an `import` to use.

```apex
import os
os.output("Hello")
```

You might wonder: why do built-in modules need an import if they're already part of the interpreter? Why not just make `os.output` available everywhere?

The answer is **clarity**. If every built-in function were available without an import, then reading any piece of code would require you to know the entire standard library by heart. You'd see `output(...)` and have to remember: is that from `os`? From `sys`? From some other module? With explicit imports, you always know where a function came from. You see `os.output` and you know it's the output function from the `os` module. You see `import os` at the top of the file and you know the file uses `os`.

This also makes it obvious when you're using features you didn't intend to. If you're reading a file that imports `crypto`, you know immediately that the file does something cryptographic. If there were no imports, you'd have to scan the whole file to discover that.

So the rule is simple: **to use a built-in module, you must import it by name**. No file path, no `.apex` extension. Just the name:

```apex
import os
import math
import string
import json
```

#### Built-in Modules Do Not End with `.apex`
This follows from the previous point. A user file is a file on disk, and its import path ends with `.apex`. A built-in module isn't a file, so its import is just the module name with no extension.

```apex
import os            // built-in, no extension
import math          // built-in, no extension
import utils/math.apex  // user file, ends with .apex
```

These are two very different kinds of import, and Apex distinguishes them by whether the path ends with `.apex`. If it does, it's a file. If it doesn't, it must be a built-in module name.

If you accidentally write `import os.apex`, Apex will try to find a file called `os.apex` on disk and fail. If you accidentally write `import math` when you meant the file `math.apex`, Apex will try to load the built-in `math` module — and probably succeed, which could be confusing. Be careful to include the `.apex` extension for user files and omit it for built-in modules.

#### Accessing Built-in Contents
Once imported, a built-in module's contents are accessed the same way as a user module's — with the module name as a prefix.

```apex
import math
result = math.sqrt(16)  // 4
```

```apex
import string
length = string.length("hello")  // 5
```

The prefix is always the module's name, exactly as you wrote it in the import.

#### One Built-in Module at a Time
You can import as many built-in modules as you want, but each needs its own `import` line:

```apex
import os
import math
import string
import json
```

You cannot write `import os, math` — Apex doesn't support that syntax. One import per line.

### Rules and Restrictions
Imports are powerful, but they come with rules. Here are the ones you need to remember.

#### 1. Import at the Top
Imports must appear at the top of the file, before any other code. You cannot import a module halfway through a function. The reason is simple: when Apex loads your file, it needs to know what modules are available before it can resolve any names. If imports were scattered throughout, the compiler would have to make multiple passes to figure everything out.

```apex
import os
import math

// rest of your code
```

If you put an import after some other code, Apex will report an error.

#### 2. Import Paths Are Relative to the Main File
We already covered this, but it's important enough to repeat. **Every import path in every file is written as if from the main file's location.** Not from the importing file's location, not from your current working directory. Just from the main file. This rule makes all imports consistent and predictable.

```apex
// Even in a deeply nested file, you write imports
// as if you were at the project root.
import helpers/math.apex
```

#### 3. `.apex` for User Files, Nothing for Built-in Modules
User file imports must end with `.apex`. Built-in module imports must not. This is the syntax that tells Apex which kind of import you mean.

```apex
import os                 // built-in
import utils/math.apex    // user file
```

#### 4. No Aliasing Built-in Modules
You cannot write `import os as system`. Built-in modules keep their canonical names. Aliases are only for user file imports.

#### 5. Aliases Must Be Unique and Valid
An alias must be a valid identifier — letters, digits, underscores, not starting with a digit. And it must not clash with any other alias or module name in the same file.

```apex
import utils/calc.apex as calc
import utils/stat.apex as stats  // fine, different alias
```

#### 6. No Multiple Modules Per Import
Each `import` line brings in exactly one module. You cannot write `import os, math`. Use two lines.

#### 7. Only Top-Level Definitions Are Imported
When you import a file, you get access to its top-level functions, variables, and constants. You do **not** get access to anything declared inside nested scopes — inside functions, inside `if` blocks, and so on.

#### 8. Module Names Come from File Names
The name you use to access a module's contents is the file's name without the `.apex` extension. The folders in the path are not part of the module's name. So `utils/math.apex` is accessed as `math`, not `utils.math`.

#### 9. Files Must Exist and Be Readable
If Apex can't find an imported file, it reports an error. The path must exist relative to the main file's folder, and the file must be readable.

#### 10. Built-in Modules Must Be Imported Before Use
Even though `os`, `math`, and the rest are built into the interpreter, you still must import them before using their contents. Without the import, `os.output("Hello")` would be reported as an undefined name.

## Conclusion
And that's it — you now know the entire Apex language. Every keyword, every data type, every operator, every control structure, every way to define and use functions, every rule about async, every detail of imports. There is no hidden syntax waiting for you later. What you have learned is the complete language. Everything else from here on out is not new syntax — it's the standard library, which is just a very large collection of functions you already know how to call.

The best next step is to open the **[Library Reference](resources/Library_Reference.md)**. It lists every built-in module — `os`, `sys`, `math`, `string`, `table`, `random`, `json`, `xml`, `csv`, `base`, `regex`, `crypto`, `zip`, `datetime` — and every function inside them, with short examples for each. You will recognize the pattern immediately: `import` the module at the top of your file, then call its functions with the module name as a prefix. Nothing new to learn — just a lot of useful tools to discover.

Don't try to memorize the Library Reference. Nobody does. Skim it once so you know what's available, then come back to it whenever you need something specific. "How do I read a file?" — check `os.read`. "How do I round a number?" — check `math.round`. "How do I sort a table?" — look in `table`. That's how everyone uses it, and that's how you should too.

**Remember:**

> *Apex is designed to be simple, but it's powerful with libraries.*

**Happy coding in Apex!**