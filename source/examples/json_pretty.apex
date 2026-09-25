// source/examples/json_pretty.apex
// Implementation of JSON Pretty-Printer on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os      // argv, file read, stdout
import json    // decode input, encode strings back with proper escaping
import table   // keys, size, membership
import string  // slice, number formatting helper

// ansi color codes via octal escapes (\033 = ESC)
// json's native palette: keys blue, strings green, numbers magenta, literals cyan
constant C_RESET   = "\033[0m"     // clears all styling
constant C_KEY     = "\033[34m"    // object keys
constant C_STRING  = "\033[32m"    // string values
constant C_NUMBER  = "\033[35m"    // numeric values
constant C_LITERAL = "\033[36m"    // true / false / null
constant C_PUNCT   = "\033[90m"    // braces, brackets, commas, colons
constant C_ERROR   = "\033[31m"    // usage and I/O errors

// indentation unit; 4 spaces matches the language's block style
constant INDENT_UNIT = "    "

// apex reserves braces for interpolation, so they cannot appear literally
// in source strings; json.encode of an empty table returns "{}", from which
// we slice each brace into its own variable
BRACES = json.encode([])              // "{}"
LBRACE = string.slice(BRACES, 1, 1)   // "{"
RBRACE = string.slice(BRACES, 2, 2)   // "}"

// builds the indentation prefix for a given depth level
function indent_str(depth)
    s = ""
    for i = 1, depth
        s = "{s}{INDENT_UNIT}"
    return s

// tells ordered lists apart from key-value objects
// apex tables are unordered, so we treat "keys are exactly 1..n" as an array
function is_array(t)
    keys = table.keys(t)
    n = table.size(keys)
    if n == 0
        return false

    for i = 1, n
        k = keys[i]
        if type(k) != "number"
            return false
        if k != i
            return false
    return true

// renders a single scalar as its json text plus color, or none for composite values
// returns none when the value is a table, so the caller descends into it
function render_scalar(v)
    t = type(v)

    if t == "none"
        return "{C_LITERAL}null{C_RESET}"

    if t == "boolean"
        if v == true
            return "{C_LITERAL}true{C_RESET}"
        else
            return "{C_LITERAL}false{C_RESET}"

    if t == "number"
        return "{C_NUMBER}{string(v)}{C_RESET}"

    if t == "string"
        return "{C_STRING}{json.encode(v)}{C_RESET}"

    return none

// renders any value: scalars through render_scalar, tables recursively
function pretty(v, indent)
    scalar = render_scalar(v)
    if scalar != none
        return scalar

    // v is a table
    if table.size(v) == 0
        return "{C_PUNCT}{LBRACE}{RBRACE}{C_RESET}"

    if is_array(v) == true
        result = "{C_PUNCT}[{C_RESET}\n"
        n = table.size(v)
        for i = 1, n
            result = "{result}{indent_str(indent + 1)}{pretty(v[i], indent + 1)}"
            if i < n
                result = "{result}{C_PUNCT},{C_RESET}"
            result = "{result}\n"
        result = "{result}{indent_str(indent)}{C_PUNCT}]{C_RESET}"
        return result

    // object: keys printed as quoted strings, values recursively
    result = "{C_PUNCT}{LBRACE}{C_RESET}\n"
    keys = table.keys(v)
    n = table.size(keys)
    for i = 1, n
        k = keys[i]
        key_str = json.encode(k)
        result = "{result}{indent_str(indent + 1)}{C_KEY}{key_str}{C_RESET}"
        result = "{result}{C_PUNCT}:{C_RESET} "
        result = "{result}{pretty(v[k], indent + 1)}"
        if i < n
            result = "{result}{C_PUNCT},{C_RESET}"
        result = "{result}\n"
    result = "{result}{indent_str(indent)}{C_PUNCT}{RBRACE}{C_RESET}"
    return result

// entry point
arg = os.args[1]
if arg == none
    os.output("{C_ERROR}Usage: apex json_pretty.apex <file.json>{C_RESET}")
    os.exit(1)

content = os.read(arg)
if content == none
    os.output("{C_ERROR}Cannot read {arg}{C_RESET}")
    os.exit(1)

data = json.decode(content)
if data == none
    os.output("{C_ERROR}Invalid JSON in {arg}{C_RESET}")
    os.exit(1)

os.output(pretty(data, 0))