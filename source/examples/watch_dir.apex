// source/examples/watch_dir.apex
// Implementation of Directory Watcher on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os     // filesystem listing and time-based polling
import table  // key lookup, iteration over folder snapshots

// polling interval in seconds; smaller = more responsive, more CPU
constant POLL_INTERVAL = 1

// ansi color codes via octal escapes (\033 = ESC)
// kept as constants so the reset sequence never drifts out of sync
constant C_RESET  = "\033[0m"    // clears all styling
constant C_GREEN  = "\033[32m"   // added entries
constant C_RED    = "\033[31m"   // removed entries
constant C_YELLOW = "\033[33m"   // banner and errors

// reads the folder and returns a set of its entries as table keys
function snapshot(path)
    items = os.list_folder(path)
    if items == none
        return none

    set = []
    for item in items
        set[item] = true
    return set

// prints added and removed entries between two snapshots
function report_changes(prev, curr)
    // any key in curr but not in prev was added
    for k in table.keys(curr)
        if table.has(prev, k) == false
            os.output("{C_GREEN}[ADDED]  {C_RESET} {k}")

    // any key in prev but not in curr was removed
    for k in table.keys(prev)
        if table.has(curr, k) == false
            os.output("{C_RED}[REMOVED]{C_RESET} {k}")
    return none

// polls the folder forever, printing changes as they appear first tick just prints the banner
async function watch_dir(path)
    prev = none             // no baseline yet
    first = true            // controls the banner on the initial tick
    running = true          // loop flag; flipped to false on any fatal error

    for running == true
        curr = snapshot(path)
        if curr == none
            os.output("{C_YELLOW}Cannot list folder: {path}{C_RESET}")
            running = false
            continue

        if first == true
            os.output("{C_YELLOW}Watching {path} (poll every {POLL_INTERVAL}s)...{C_RESET}")
            first = false
        else
            report_changes(prev, curr)

        prev = curr
        await os.wait(POLL_INTERVAL)
    return none

// entry point
await watch_dir(".")