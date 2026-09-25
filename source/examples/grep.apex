// source/examples/grep.apex
// Implementation of Recursive Pattern Search on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os      // recursive listing, file read, path checks
import regex   // search each line against the user pattern
import table   // size, keys, membership
import string  // split, length

// ansi color codes via octal escapes (\033 = ESC)
constant C_RESET   = "\033[0m"     // clears all styling
constant C_FILE    = "\033[36m"    // file path prefix
constant C_LINE    = "\033[32m"    // line number
constant C_MATCH   = "\033[31m"    // matching line content
constant C_SUMMARY = "\033[33m"    // final match count
constant C_ERROR   = "\033[31m"    // usage and I/O errors

// shared counters; recursion cannot reassign outer names, but mutating
// table fields is allowed and works across call frames
state = ["matches" = 0, "files_scanned" = 0, "files_matched" = 0]

// returns a sorted list of entries under path, skipping "." and ".."
// string-keyed set gives table.keys its lexicographic ordering for free
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

// true when content looks binary
// string.length stops at the first null byte, so a binary file's readable
// char count collapses relative to its raw byte size; valid utf-8 text uses
// at most 4 bytes per char, so clen * 4 < size flags binary files reliably
function is_binary(path, content)
    sz = os.size(path)
    if sz == none
        return false
    clen = string.length(content)
    return clen * 4 < sz

// scans one file, printing every matching line; updates state counters
function search_file(path, pattern, opts)
    content = os.read(path)
    if content == none
        return none

    if is_binary(path, content) == true
        return none

    state["files_scanned"] = state["files_scanned"] + 1

    lines = string.split(content, "\n")
    file_hit = false

    for i = 1, table.size(lines)
        line = lines[i]

        // regex.search returns an empty table when nothing matched
        hit = regex.search(pattern, line, opts)
        if table.size(hit) == 0
            continue

        if file_hit == false
            file_hit = true
            state["files_matched"] = state["files_matched"] + 1

        state["matches"] = state["matches"] + 1
        os.output("{C_FILE}{path}{C_RESET}:{C_LINE}{i}{C_RESET}: {C_MATCH}{line}{C_RESET}")
    return none

// recurses through path, invoking search_file on every regular file
function walk(path, pattern, opts)
    if os.is_folder(path) == true
        children = list_sorted(path)
        if children == none
            return none
        for name in children
            walk("{path}/{name}", pattern, opts)
        return none

    if os.is_file(path) == true
        search_file(path, pattern, opts)
    return none

// entry point: grep.apex <pattern> [path] [-i]
args = os.args()
if table.size(args) == 0
    os.output("{C_ERROR}Usage: apex grep.apex <pattern> [path] [-i]{C_RESET}")
    os.exit(1)

pattern = args[1]

// first non-flag positional becomes the search path; remaining flags are options
path = none
opts = []
for i = 2, table.size(args)
    if args[i] == "-i"
        opts["ignorecase"] = true
    else if path == none
        path = args[i]

if path == none
    path = "."

if os.exists(path) == false
    os.output("{C_ERROR}No such path: {path}{C_RESET}")
    os.exit(1)

walk(path, pattern, opts)

os.output("")
os.output("{C_SUMMARY}{state['matches']} match(es) in {state['files_matched']} of {state['files_scanned']} file(s){C_RESET}")