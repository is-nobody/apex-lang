# Apex: Pain-Free Programming
## Table of Contents
### Introduction
- [Preface](#section)
- [What is a "programming language"?](#section)
- [A bit of history about Apex](#section)
- [Preparation for development](#section)
  - [Installing the Apex Language](#section)
  - [Installing the Apex Code](#section)
- [First Program](#section)

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
- [Else-If Statement](#else-if-statement)
- [Else Statement](#else-statement)
- [Ternary Expression](#ternary-expression)

### Match / Case
- [Match Statement](#match-statement)
- [Case Patterns](#case-patterns)
- [Default Case](#default-case)
- [Rules and Restrictions](#rules-and-restrictions)

### For Loops
- [For Counter](#for-counter)
- [For Table Iteration](#for-table-iteration)
- [For Condition](#for-condition)
- [Break](#break)
- [Continue](#continue)

### Functions
- [Why Functions Exist](#why-functions-exist)
- [Function Statement](#function-statement)
- [Parameters](#parameters)
- [Return Value](#return-value)
- [Call](#call)
- [Scope and Blocks](#scope-and-blocks)
- [Early Return](#early-return)
- [Recursion](#recursion)

### Async / Await
- [Why Async Exists](#why-async-exists)
- [What Is a Future](#what-is-a-future)
- [Async Function](#async-function)
- [Await](#await)
- [Where Await Can Be Used](#where-await-can-be-used)
- [What Can Be Awaited](#what-can-be-awaited)
- [Running in the Background](#running-in-the-background)

### Imports
- [Importing an Entire File](#section)
- [Importing from Sub-folders](#section)
- [Importing from One Sub-folder into Another](#section)

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

Important: The conditions are checked in order. Once a condition is true, the rest are ignored. So you should order your conditions from most specific to least specific, or from highest to lowest, as in the grade example.

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

### Ternary Exptession
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

Important: The ternary is meant for simple two-way choices. You cannot chain them or use more than one condition. If you need to check more than one condition, use a regular `if`/`else if`/`else` statement. The ternary is a convenience, not a replacement for full if statements.

Let's summarize the ternary with an example that uses a boolean variable:

```apex
is_member = true
price = 100
final_price = price * 0.9 if is_member == true else price
```

Here, if `is_member` is `true`, `final_price` is 90. If `is_member` is `false`, `final_price` is 100.

Remember: you must write `is_member == true`, not just `is_member`. Apex requires explicit boolean comparisons.

That's the ternary. It's a compact way to write a simple if-else that returns a value.

Now you know how to make your programs decide! Use `if` for a single condition, `else if` for multiple conditions, `else` for the default case, and the ternary for simple two-way choices. All conditions must be boolean, and indentation defines the blocks. In the next section, we'll learn how to repeat code with loops.

## Match / Case
Sometimes you have a single value that you need to compare against many different possibilities. You could write a long chain of `if` and `else if` statements, but that gets messy quickly. Apex gives you a cleaner tool for this exact situation: the `match` statement.

Think of `match` as a specialized decision-maker. You give it one value—the subject—and then you list a series of constant patterns. Apex checks the subject against each pattern in order. As soon as it finds a match, it runs the corresponding block of code and then skips the rest of the `match`. It’s like a multi-way fork in the road, but much more readable than a pile of `else if`s.

`match` is not an expression. It doesn’t produce a value you can assign. It’s a statement, just like `if`. You use it when you want to *do* different things based on a value, not when you want to compute a result.

### Match Statement
The `match` keyword is followed by the value you want to check—the subject. Then you write an indented block containing `case` branches. Each `case` has a constant pattern, and below it (indented further) is the code that runs when the subject equals that pattern.

Syntax:
```apex
match subject
    case pattern1
        // code to run if subject equals pattern1
    case pattern2
        // code to run if subject equals pattern2
    // ... more cases ...
```

Let’s look at a simple example. Suppose you have a numeric status code and you want to set a message based on it.

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

After this runs, `message` is `"Not Found"`. Here’s what happens:
- Apex looks at `status`, which is `404`.
- It checks `case 200`: 404 is not 200, so it moves on.
- It checks `case 404`: 404 equals 404, so it runs the block `message = "Not Found"`.
- It then skips the remaining cases (there’s only `case 500` left, which is ignored).

If `status` were `200`, `message` would be `"OK"`. If `status` were `500`, `message` would be `"Server Error"`. If `status` were something else, like `302`, none of the cases would match, and `message` would stay `""`.

Notice the indentation. The `match` line is at the current indentation. The `case` lines are indented four spaces. The code inside each case is indented another four spaces. This is how Apex knows which code belongs to which case. Just like with `if`, indentation defines the blocks.

You can have as many `case` branches as you need. They are checked from top to bottom. The first one that matches wins, and the rest are ignored.

### Case Patterns
A pattern is the value you compare against. It must be a **constant**—something that never changes. You cannot use a variable as a pattern, because the whole point of `match` is to compare against fixed, known values.

The allowed constant patterns are:
- **Number literals**: like `42`, `3.14`, or negative numbers like `-1`.
- **String literals**: like `"hello"`, `"error"`, or `""` (empty string).
- **Boolean literals**: `true` or `false`.
- **None**: the special value `none`.

Here’s an example with string patterns:

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

The type of the pattern must match the type of the subject. You cannot match a number against a string pattern. If you try, Apex will warn you that the case can never match. So if your subject is a number, all patterns must be numbers. If it’s a string, all patterns must be strings, and so on.

### Default Case
What if none of the patterns match? You can provide a default case that runs when nothing else matches. A default case is written as `case` with no value after it. It’s like the `else` in an if-else chain.

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

After this, `description` is `"Unknown status"` because `302` didn’t match `200` or `404`, so the default case ran.

If you omit the default case and no pattern matches, then the `match` statement simply does nothing. Execution continues with the code after the `match`. That might be fine if you only care about specific values. But if you want to handle “everything else,” use a default case.

The default case must come last. If you put any case after it, Apex will report an error. That’s because once you have a default, any case below it would be unreachable—the default would always run first.

### Rules and Restrictions
To use `match` correctly, keep these rules in mind:

1. **Subject type**: The subject must be a `number`, `string`, `boolean`, or `none`. You cannot match against a table or any other complex type. If you try, Apex will tell you it’s not allowed.

2. **Pattern type**: Each pattern must be a constant of the same type as the subject. You cannot mix types. For example, if the subject is a number, you cannot use a string pattern. Apex will warn you that the pattern can never match.

3. **Constants only**: Patterns must be literal constants. You cannot use variables, expressions, or function calls as patterns. For instance, `case x` where `x` is a variable is not allowed.

4. **Order matters**: Cases are checked from top to bottom. The first matching case wins. Once a case runs, the rest of the `match` is skipped. There is no “fall-through” like in some other languages’ switch statements.

5. **Default case**: You may have at most one default case, written as `case` with no value. It must be the last case. If no case matches and there is no default, the `match` does nothing.

6. **Scope**: Each `case` body has its own scope. Variables declared inside a case are local to that case and are not visible after the `match`. You can reuse the same variable name in different cases without conflict.

7. **Not an expression**: `match` is a statement, not an expression. It does not produce a value. You cannot write `x = match ...` or use it inside another expression. Use it only when you want to execute different blocks of code based on a value.

8. **No tables**: Tables cannot be used as subjects or patterns. Only the four simple types are allowed.

`match` is a powerful way to keep your code clean when you have many fixed options to check. It’s especially handy for things like status codes, command strings, or simple state machines. Just remember to keep your patterns constant, your types consistent, and your default case (if you need one) at the end.

Now you have another tool in your decision-making toolkit. In the next section, we’ll learn how to repeat code with loops.

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

#### Stepping by More Than One
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

If you use a negative step, the start value should be greater than the end value for the loop to run at all. If you swap them and still use a negative step, the loop won't run.

**The step cannot be zero.** If you write `for i = 1, 10, 0`, Apex will report an error. A step of zero would mean the loop variable never changes, so the loop would never end — that's not allowed.

You can also use decimals as steps, but be careful. Floating-point arithmetic can introduce tiny rounding errors. For most counting tasks, whole-number steps are what you want.

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

Now consider a key-value table:

```apex
user = ["name" = "Alice", "age" = 30, "city" = "Dubai"]
result = ""
for value in user
    result = "{result}{value} "
```

Here, `result` becomes `"30 Dubai Alice "` (or some other order — key-value tables do not guarantee the order in which values are visited). The loop visits the values `"Alice"`, `30`, and `"Dubai"`, but the order depends on the internal layout of the table. If you need a specific order, you should sort or restructure your data first.

The key thing to remember: **for table iteration gives you the values, not the keys**. If you want the keys, Apex provides ways to get them, but that's a topic for later.

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

Here's an example that counts down:

```apex
counter = 5
result = ""
for counter >= 1
    result = "{result}{counter}"
    counter = counter - 1
```

After this, `result` is `"54321"`.

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

`break` only exits the **innermost** loop. If you have a loop inside another loop, `break` inside the inner loop exits only that inner loop. The outer loop continues. To exit the outer loop too, you'd need a `break` in the outer loop, or some other mechanism. But nested loops are a topic for later — for now, just remember that `break` exits the loop it appears in.

You can use `break` with any form of `for`. It's especially common with condition loops, where you're looping until something happens, and then you break when it does.

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

Like `break`, `continue` affects only the innermost loop. In nested loops, `continue` skips to the next iteration of the inner loop, not the outer one.

`continue` can be used with any form of `for`. It's a clean way to say "I'm done with this item, move on to the next."

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