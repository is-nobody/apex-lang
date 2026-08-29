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
- [None](#section)
- [Tables](#section)
  - [Ordered Lists](#section)
  - [Key-Value Pairs](#section)
  - [Mixed Tables](#section)
  - [Tables Inside Tables](#section)
- [Constant](#section)
- [Built-in Functions](#section)

### Operators
- [Arithmetic Operators](#section)
- [Comparison Operators](#section)
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