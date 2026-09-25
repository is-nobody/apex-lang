// source/examples/dup_finder.apex
// Implementation of Duplicate File Finder on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os      // recursive directory listing, file read, size
import sys     // elapsed time for the final report
import math    // round() for sizes and seconds
import crypto  // sha256 content hashing
import table   // grouping paths by hash, size, membership
import string  // slice() for hex prefixes

// ansi color codes via octal escapes (\033 = ESC)
constant C_RESET = "\033[0m"     // clears all styling
constant C_PATH  = "\033[36m"    // file paths in each group
constant C_HASH  = "\033[90m"    // short sha-256 shown next to the group header
constant C_GROUP = "\033[33m"    // group header line
constant C_SAVED = "\033[32m"    // reclaimable-bytes summary
constant C_ERROR = "\033[31m"    // usage and I/O errors

// how many hex chars of the sha-256 to show in the header
constant HASH_PREFIX_LEN = 12

// walks `dir` recursively and appends every regular file path into `out`
// os.list_folder does not descend into subdirectories, so recursion is required
function collect_files(dir, out)
    items = os.list_folder(dir)
    if items == none
        return none

    for name in items
        if name == "." or name == ".."
            continue
        full = "{dir}/{name}"
        if os.is_folder(full) == true
            collect_files(full, out)
        else if os.is_file(full) == true
            out[table.size(out) + 1] = full
    return none

// formats a byte count as B / KB / MB / GB with one decimal
function human_size(bytes)
    if bytes < 1024
        return "{bytes} B"
    if bytes < 1024 * 1024
        return "{math.round(bytes / 1024, 1)} KB"
    if bytes < 1024 * 1024 * 1024
        return "{math.round(bytes / (1024 * 1024), 1)} MB"
    return "{math.round(bytes / (1024 * 1024 * 1024), 2)} GB"

// entry point: path comes from argv, defaults to the current directory
arg = os.args[1]
if arg == none
    arg = "."

if os.exists(arg) == false
    os.output("{C_ERROR}No such path: {arg}{C_RESET}")
    os.exit(1)

t0 = sys.time()

files = []
collect_files(arg, files)

if table.size(files) == 0
    os.output("{C_ERROR}No files found under {arg}{C_RESET}")
    os.exit(0)

// build a hash -> [paths...] map; identical content => identical sha-256
// nested tables are references, so appending to `bucket` mutates by_hash[h]
by_hash = []
for path in files
    content = os.read(path)
    if content == none
        continue    // unreadable file (permissions, special file): skip quietly

    h = crypto.sha256(content)
    if table.has(by_hash, h) == false
        by_hash[h] = []
    bucket = by_hash[h]
    bucket[table.size(bucket) + 1] = path

// report every group with more than one file
total_dupes = 0     // files beyond the first in each group
total_saved = 0     // bytes recoverable by keeping one per group

for h in table.keys(by_hash)
    group = by_hash[h]
    n = table.size(group)
    if n < 2
        continue

    total_dupes = total_dupes + (n - 1)
    short = string.slice(h, 1, HASH_PREFIX_LEN)
    os.output("{C_GROUP}--- {n} copies  sha256:{C_HASH}{short}...{C_RESET}")

    // every file in a group is byte-identical, so one size describes them all
    size = os.size(group[1])
    for i = 1, n
        os.output("  {C_PATH}{group[i]}{C_RESET}  ({human_size(size)})")

    total_saved = total_saved + size * (n - 1)

elapsed = sys.time() - t0
os.output("")

if total_dupes == 0
    os.output("No duplicates found under {arg}.")
else
    os.output("{C_SAVED}{total_dupes} duplicate(s); reclaimable: {human_size(total_saved)}{C_RESET}")

os.output("Scanned {table.size(files)} file(s) in {math.round(elapsed, 2)}s.")