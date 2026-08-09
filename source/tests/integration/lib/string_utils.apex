// source/tests/integration/lib/string_utils.apex.apex
// Implementation of String Utility Functions for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import string

// helper: reverses a string by iterating from end to start
function reverse(text)
    result = ""
    len = string.length(text)
    for i = 1, len, 1
        pos = len - i + 1
        char = string.slice(text, pos, pos)
        result = "{result}{char}"
    return result

// helper: converts string to uppercase via standard library
function to_upper(text)
    return string.upper(text)

// helper: converts string to lowercase via standard library
function to_lower(text)
    return string.lower(text)

// helper: returns the length of a string
function get_length(text)
    return string.length(text)

// helper: repeats a string a specified number of times
function repeat(text, count)
    result = ""
    i = 0
    for i = 1, count
        result = "{result}{text}"
    return result

// helper: concatenates two strings using interpolation
function concat(a, b)
    return "{a}{b}"