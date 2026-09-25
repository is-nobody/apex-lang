// source/examples/hexdump.apex
// Implementation of Hex Dump Utility on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os      // argv, file read, stdout
import base    // encode_16 / decode_16 for byte <-> hex conversion
import table   // printable-set membership
import math    // round_down for hex digit extraction
import string  // length, slice

// ansi color codes via octal escapes (\033 = ESC)
constant C_RESET  = "\033[0m"     // clears all styling
constant C_OFFSET = "\033[90m"    // offset column
constant C_HEX    = "\033[36m"    // hex byte pane
constant C_ASCII  = "\033[33m"    // printable characters in the ASCII pane
constant C_ERROR  = "\033[31m"    // usage and I/O errors

// classic hexdump layout: 16 bytes per row, 8-wide offset covers files up to 4 GiB
constant BYTES_PER_ROW = 16
constant OFFSET_WIDTH  = 8
constant HEXDIGITS     = "0123456789abcdef"

// formats a non-negative integer as lowercase hex
// apex has no builtin number->hex conversion, so digits are peeled off manually
function num_to_hex(n)
    if n == 0
        return "0"
    result = ""
    for n > 0
        digit = n - math.round_down(n / 16) * 16
        result = "{string.slice(HEXDIGITS, digit + 1, digit + 1)}{result}"
        n = math.round_down(n / 16)
    return result

// left-pads a string with '0' up to width characters
function pad_left(s, width)
    for string.length(s) < width
        s = "0{s}"
    return s

// builds the set of hex pairs corresponding to printable ASCII (0x20..0x7e)
// membership then costs one table.has per byte instead of a range check
function build_printable_set()
    set = []
    for c = 32, 126
        hi = math.round_down(c / 16)
        lo = c - hi * 16
        hs = string.slice(HEXDIGITS, hi + 1, hi + 1)
        ls = string.slice(HEXDIGITS, lo + 1, lo + 1)
        set["{hs}{ls}"] = true
    return set

// renders one row: offset, hex pane, ascii pane
function render_row(offset, hex_str, byte_count, printable_set)
    hex_part = ""
    ascii_part = ""

    for j = 0, BYTES_PER_ROW - 1
        pos = offset + j
        if pos < byte_count
            pair = string.slice(hex_str, pos * 2 + 1, pos * 2 + 2)
            hex_part = "{hex_part} {pair}"

            if table.has(printable_set, pair) == true
                ascii_part = "{ascii_part}{base.decode_16(pair)}"
            else
                ascii_part = "{ascii_part}."
        else
            hex_part = "{hex_part}   "      // keep column alignment on the last row
            ascii_part = "{ascii_part} "

    off = pad_left(num_to_hex(offset), OFFSET_WIDTH)
    return "{C_OFFSET}{off}{C_RESET}  {C_HEX}{hex_part}{C_RESET}  {C_ASCII}|{ascii_part}|{C_RESET}"

// entry point
arg = os.args[1]
if arg == none
    os.output("{C_ERROR}Usage: apex hexdump.apex <file>{C_RESET}")
    os.exit(1)

if os.exists(arg) == false
    os.output("{C_ERROR}No such file: {arg}{C_RESET}")
    os.exit(1)

if os.is_file(arg) == false
    os.output("{C_ERROR}Not a regular file: {arg}{C_RESET}")
    os.exit(1)

content = os.read(arg)
if content == none
    os.output("{C_ERROR}Cannot read {arg}{C_RESET}")
    os.exit(1)

// base.encode_16 walks the raw byte sequence, so hex length / 2 gives the
// exact byte count regardless of whether content is ascii, utf-8, or binary
hex_str = base.encode_16(content)
byte_count = math.round_down(string.length(hex_str) / 2)

if byte_count == 0
    os.output("{C_ERROR}Empty file: {arg}{C_RESET}")
    os.exit(0)

printable_set = build_printable_set()

offset = 0
for offset < byte_count
    os.output(render_row(offset, hex_str, byte_count, printable_set))
    offset = offset + BYTES_PER_ROW

os.output("")
os.output("{C_OFFSET}{byte_count} byte(s){C_RESET}")