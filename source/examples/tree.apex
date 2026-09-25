// source/examples/tree.apex
// Implementation of Directory Tree Printer on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os      // list_folder, is_folder, size, exists
import table   // size, keys, membership
import math    // round for size formatting
import string  // length for argument check

// ansi color codes via octal escapes (\033 = ESC)
constant C_RESET = "\033[0m"     // clears all styling
constant C_DIR   = "\033[34m"    // directory names
constant C_FILE  = "\033[37m"    // file names
constant C_TREE  = "\033[90m"    // tree connector glyphs
constant C_INFO  = "\033[36m"    // file size suffix
constant C_HEAD  = "\033[1m"     // root path header
constant C_SUM   = "\033[33m"    // summary line
constant C_ERROR = "\033[31m"    // usage and I/O errors

// tree glyphs; four variants so connector columns line up on any depth
constant GLYPH_TEE   = "├── "     // branch with more siblings to come
constant GLYPH_LAST  = "└── "     // final entry in its parent
constant GLYPH_PIPE  = "│   "     // continuation under a non-last sibling
constant GLYPH_BLANK = "    "     // blank continuation under a last sibling

// toggles the size suffix next to each file; directories never carry one
constant SHOW_FILE_SIZE = true

// shared counters; recursion cannot reassign outer names, so fields are mutated
stats = ["dirs" = 1, "files" = 0]     // root directory already counted

// formats a byte count as B / K / M / G with one decimal
function human_size(bytes)
    if bytes < 1024
        return "{bytes}B"
    if bytes < 1024 * 1024
        return "{math.round(bytes / 1024, 1)}K"
    if bytes < 1024 * 1024 * 1024
        return "{math.round(bytes / (1024 * 1024), 1)}M"
    return "{math.round(bytes / (1024 * 1024 * 1024), 2)}G"

// returns a lexicographically sorted list of entry names under path
// filters out "." and ".." which os.list_folder passes through from the OS
// the string-keyed set is what gives table.keys its lexicographic order
function list_sorted(path)
    raw = os.list_folder(path)
    if raw == none
        return none

    set = []
    for item in raw
        if item == "." or item == ".."
            continue
        set[item] = true
    return table.keys(set)

// recursively prints one directory subtree
// prefix carries connector glyphs inherited from the parent chain
// is_last says whether this entry is the final child of its parent
function walk(path, name, prefix, is_last)
    connector = GLYPH_TEE
    if is_last == true
        connector = GLYPH_LAST

    if os.is_folder(path) == true
        stats["dirs"] = stats["dirs"] + 1
        os.output("{C_TREE}{prefix}{connector}{C_RESET}{C_DIR}{name}/{C_RESET}")

        children = list_sorted(path)
        if children == none
            return none

        // children inherit a longer prefix: a pipe if more siblings follow,
        // otherwise four blanks so the connectors stay aligned
        child_prefix = "{prefix}{GLYPH_PIPE}"
        if is_last == true
            child_prefix = "{prefix}{GLYPH_BLANK}"

        n = table.size(children)
        for i = 1, n
            child_name = children[i]
            child_path = "{path}/{child_name}"
            child_is_last = (i == n)
            walk(child_path, child_name, child_prefix, child_is_last)
        return none

    // leaf: regular file, socket, symlink, anything non-directory
    stats["files"] = stats["files"] + 1

    suffix = ""
    if SHOW_FILE_SIZE == true
        sz = os.size(path)
        if sz != none
            suffix = " {C_INFO}{human_size(sz)}{C_RESET}"

    os.output("{C_TREE}{prefix}{connector}{C_RESET}{C_FILE}{name}{C_RESET}{suffix}")
    return none

// entry point: path from argv, defaults to the current directory
arg = os.args[1]
if arg == none
    arg = "."

if os.exists(arg) == false
    os.output("{C_ERROR}No such path: {arg}{C_RESET}")
    os.exit(1)

// single-file mode: print one line and stop, tree layout does not apply
if os.is_folder(arg) == false
    suffix = ""
    sz = os.size(arg)
    if sz != none
        suffix = " {C_INFO}{human_size(sz)}{C_RESET}"
    os.output("{C_FILE}{arg}{C_RESET}{suffix}")
    os.exit(0)

os.output("{C_HEAD}{arg}{C_RESET}")

children = list_sorted(arg)
if children == none
    os.output("{C_ERROR}Cannot list {arg}{C_RESET}")
    os.exit(1)

n = table.size(children)
for i = 1, n
    child_name = children[i]
    child_path = "{arg}/{child_name}"
    child_is_last = (i == n)
    walk(child_path, child_name, "", child_is_last)

os.output("")
os.output("{C_SUM}{stats['dirs']} director(ies), {stats['files']} file(s){C_RESET}")