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
- [Logical Operators](#section)

### If Statements
- [If Statement](#section)
- [Else-If Statement](#section)
- [Else Statement](#section)
- [Ternary Statement](#section)

### For Loops
- [For Counter](#section)
- [For Table Iteration](#section)
- [For Condition](#section)
- [Break](#section)
- [Continue](#section)

### Functions
- [Function Statement](#section)
- [Parameters](#section)
- [Return Value](#section)
- [Call](#section)

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