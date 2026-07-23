import string

function reverse(text)
    result = ""
    len = string.length(text)
    for i = 1, len, 1
        pos = len - i + 1
        char = string.sub(text, pos - 1, pos)
        result = "{result}{char}"
    return result

function to_upper(text)
    return string.upper(text)

function to_lower(text)
    return string.lower(text)

function get_length(text)
    return string.length(text)

function repeat(text, count)
    result = ""
    i = 0
    for i = 1, count
        result = "{result}{text}"
    return result

function concat(a, b)
    return "{a}{b}"