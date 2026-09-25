// source/examples/csv_stats.apex
// Implementation of CSV Column Statistics on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os     // argv, file read, stdout
import csv    // decode input into a table of row tables
import table  // keys, size, membership

// ansi color codes via octal escapes (\033 = ESC)
constant C_RESET   = "\033[0m"     // clears all styling
constant C_HEAD    = "\033[36m"    // column name in report lines
constant C_NUMBER  = "\033[35m"    // numeric metric values
constant C_NONE    = "\033[90m"    // "no numeric values" placeholder
constant C_ERROR   = "\033[31m"    // usage and I/O errors

// entry point
arg = os.args[1]
if arg == none
    os.output("{C_ERROR}Usage: apex csv_stats.apex <file.csv>{C_RESET}")
    os.exit(1)

content = os.read(arg)
if content == none
    os.output("{C_ERROR}Cannot read {arg}{C_RESET}")
    os.exit(1)

data = csv.decode(content)
if data == none
    os.output("{C_ERROR}Invalid CSV in {arg}{C_RESET}")
    os.exit(1)

if table.size(data) == 0
    os.output("{C_ERROR}Empty CSV{C_RESET}")
    os.exit(0)

// column set comes from the first row, which csv.decode builds from header names
// table.keys returns them sorted: numbers first, then strings lexicographically
first_row = data[1]
columns = table.keys(first_row)

for col in columns
    sum = 0                 // running total of numeric cells in this column
    count = 0               // how many numeric cells were seen
    min_val = none          // none until the first numeric cell arrives
    max_val = none

    for i = 1, table.size(data)
        row = data[i]
        if table.has(row, col) == true
            val = row[col]
            // csv.decode auto-converts numeric-looking strings, so type() is the filter
            if type(val) == "number"
                sum = sum + val
                count = count + 1

                if min_val == none or val < min_val
                    min_val = val
                if max_val == none or val > max_val
                    max_val = val

    if count > 0
        avg = sum / count
        os.output("{C_HEAD}{col}{C_RESET}: count={C_NUMBER}{count}{C_RESET}, avg={C_NUMBER}{avg}{C_RESET}, min={C_NUMBER}{min_val}{C_RESET}, max={C_NUMBER}{max_val}{C_RESET}")
    else
        os.output("{C_HEAD}{col}{C_RESET}: {C_NONE}no numeric values{C_RESET}")