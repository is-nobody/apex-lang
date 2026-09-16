// source/examples/task_tracker.apex
// Implementation of Interactive Task Tracker for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// each import pulls in a built-in library; after this line the
// library prefix (os., sys., etc.) becomes available in the whole file
import os      // filesystem, stdout, stdin, process control
import sys     // platform name, apex version
import json    // serialize/deserialize the task store
import random  // deterministic tag generation
import string  // trim, lowercase, split, join, length
import table   // size, remove
import math    // round() for percentage display

// constants cannot be reassigned after declaration; the compiler
// rejects any later assignment with "cannot reassign constant"
constant DATA_FILE = "tasks.json"          // where the store lives on disk
constant PRIORITIES = ["low", "normal", "high"]  // allowed priority values
constant MAX_TITLE = 60                    // upper bound for a task title

// these are loaded from disk at startup and saved back on exit
tasks = []      // map: task_id -> task table
next_id = 1     // monotonically increasing id for new tasks

// load the task store from DATA_FILE
// silently falls back to an empty store if the file is missing,
// unreadable, or contains malformed json
function load_tasks()
    // no save file yet: this is a fresh install
    if os.exists(DATA_FILE) == false
        os.output("No saved tasks found. Starting fresh.")
        return none

    // read the whole file as a single string
    raw = os.read(DATA_FILE)
    if raw == none
        os.output("Could not read {DATA_FILE}.")
        return none

    // parse json; decode returns none on any syntax error
    parsed = json.decode(raw)
    if parsed == none
        os.output("Corrupted save file. Starting fresh.")
        return none

    // restore both globals from the parsed payload
    tasks = parsed["tasks"]
    next_id = parsed["next_id"]
    os.output("Loaded {table.size(tasks)} task(s) from disk.")
    return none

// write the current task store back to DATA_FILE as json
// returns true on success, false if encoding or writing failed
function save_tasks()
    // bundle both globals into one object so a single json.encode
    // call captures the entire state
    payload = ["tasks" = tasks, "next_id" = next_id]
    encoded = json.encode(payload)
    if encoded == none
        os.output("Save failed.")
        return false

    // os.write creates the file if missing, truncates it if present
    ok = os.write(DATA_FILE, encoded)
    if ok == true
        os.output("Saved to {DATA_FILE}.")
    else
        os.output("Write failed.")
    return ok

// linear scan for a task by id; returns the numeric key in `tasks`
// or -1 when no task matches
function find_index(task_id)
    for i = 1, table.size(tasks)
        if tasks[i]["id"] == task_id
            return i
    return -1

// build a stable three-letter tag plus a random three-digit suffix
// the tag depends only on the title, so the same title always gets
// the same prefix even across runs
function tag_for(title)
    // rolling hash over character codes; the initial value and
    // multiplier are arbitrary but produce good spread for short titles
    seed = 2166136261
    parts = string.split(title, "")

    // map each character to a numeric code; apex has no builtin
    // char-to-code, so match is the only portable way to do this
    for ch in parts
        code = 0
        match ch
            case "a"
                code = 97
            case "b"
                code = 98
            case "c"
                code = 99
            case "d"
                code = 100
            case "e"
                code = 101
            case "f"
                code = 102
            case "g"
                code = 103
            case "h"
                code = 104
            case "i"
                code = 105
            case "j"
                code = 106
            case "k"
                code = 107
            case "l"
                code = 108
            case "m"
                code = 109
            case "n"
                code = 110
            case "o"
                code = 111
            case "p"
                code = 112
            case "q"
                code = 113
            case "r"
                code = 114
            case "s"
                code = 115
            case "t"
                code = 116
            case "u"
                code = 117
            case "v"
                code = 118
            case "w"
                code = 119
            case "x"
                code = 120
            case "y"
                code = 121
            case "z"
                code = 122
            case " "
                code = 32
            case
                code = 63
        // keep the seed bounded so it stays a small integer
        seed = (seed * 31 + code) % 100000

    // seed the generator, then draw a deterministic suffix
    random.seed(seed)
    letters = string.upper(string.slice("abcdefghijklmnop", 1, 3))
    return "{letters}-{random.integer(100, 999)}"

// validate inputs, build a task table, store it under the next id
// returns none; all feedback goes to stdout
function add_task(title, priority)
    // reject empty titles early; the user should retry
    if string.length(title) == 0
        os.output("Title cannot be empty.")
        return none

    // cap the title so the list output stays readable
    if string.length(title) > MAX_TITLE
        os.output("Title too long (max {MAX_TITLE} chars).")
        return none

    // verify the priority is one of the allowed values
    valid = false
    for p in PRIORITIES
        if p == priority
            valid = true
            break

    if valid == false
        os.output("Unknown priority '{priority}'. Use low/normal/high.")
        return none

    // construct the task record; "done" starts false
    task = [
        "id" = next_id,
        "title" = title,
        "priority" = priority,
        "done" = false,
        "tag" = tag_for(title)
    ]

    // store under the numeric id and advance the counter
    tasks[next_id] = task
    next_id = next_id + 1
    os.output("Added: [{task['tag']}] {title} ({priority})")
    return none

// mark a task as done; idempotent — already-done tasks report
// a message and return false instead of flipping the flag again
function complete_task(task_id)
    idx = find_index(task_id)
    if idx == -1
        os.output("Task {task_id} not found.")
        return false

    // already done: nothing to do, but tell the user
    if tasks[idx]["done"] == true
        os.output("Task {task_id} is already done.")
        return false

    // flip the flag in place; tables are reference types, so the
    // change is visible through the global `tasks` map immediately
    tasks[idx]["done"] = true
    os.output("Completed: {tasks[idx]['title']}")
    return true

// remove a task from the store; returns false if the id is unknown
function delete_task(task_id)
    idx = find_index(task_id)
    if idx == -1
        os.output("Task {task_id} not found.")
        return false

    // capture the title for the confirmation message before removal
    removed = tasks[idx]["title"]
    table.remove(tasks, task_id)
    os.output("Deleted: {removed}")
    return true

// print all tasks matching a filter: "all", "todo", or "done"
// unknown filters silently match nothing
function list_tasks(filter)
    // collect matching tasks into a fresh table keyed by task id
    visible = []
    for task in tasks
        keep = false
        if filter == "all"
            keep = true
        else if filter == "done" and task["done"] == true
            keep = true
        else if filter == "todo" and task["done"] == false
            keep = true

        // store a reference to the task; tables are shared, not copied
        if keep == true
            visible[task["id"]] = task

    // no matches: say so and stop
    if table.size(visible) == 0
        os.output("Nothing to show for filter '{filter}'.")
        return none

    os.output("---- {filter} tasks ----")

    // print one line per task; "x" marks completed tasks
    for task in visible
        mark = " "
        if task["done"] == true
            mark = "x"
        os.output("  [{mark}] #{task['id']} <{task['priority']}> {task['title']}")
    return none

// print a short summary: completion percentage and per-priority counts
function stats()
    total = table.size(tasks)
    if total == 0
        os.output("No tasks yet.")
        return none

    // accumulate counts in two passes over the same loop
    done_count = 0
    by_priority = ["low" = 0, "normal" = 0, "high" = 0]

    for task in tasks
        if task["done"] == true
            done_count = done_count + 1
        p = task["priority"]
        by_priority[p] = by_priority[p] + 1

    // round to one decimal so the output stays short
    percent = (done_count * 100) / total
    percent = math.round(percent, 1)

    os.output("Progress: {done_count}/{total} ({percent}%)")
    os.output("  low:    {by_priority['low']}")
    os.output("  normal: {by_priority['normal']}")
    os.output("  high:   {by_priority['high']}")
    return none

// write a markdown report of all tasks to report.md
// declared async so callers must use `await`; this makes the
// artificial pause below feel natural and keeps the api uniform
async function export_report()
    // simulate disk latency without blocking the event loop
    os.wait(0.2)

    // build the report line by line, then join with newlines
    lines = ["# Task Report", ""]
    for task in tasks
        status = "OPEN"
        if task["done"] == true
            status = "DONE"
        lines[table.size(lines) + 1] = "- [{status}] {task['title']} ({task['priority']})"

    report = string.join(lines, "\n")
    os.write("report.md", report)

    // return the number of task lines (excludes the header and blank)
    return table.size(lines) - 2

// read one line from stdin, trim whitespace
// returns none on eof so callers can stop cleanly instead of
// spinning on empty input
function prompt(question)
    answer = os.input("{question} ")
    if answer == none
        return none
    return string.trim(answer)

// main repl loop: print a menu, read a command, dispatch, repeat
// declared async so the export case can `await export_report()`
async function main_loop()
    running = true
    for running == true
        // blank line separates iterations for readability
        os.output("")
        os.output("Commands: add | list | done | del | stats | save | export | quit")
        raw = prompt(">")

        // eof or closed stdin: stop the loop instead of spinning
        if raw == none
            os.output("(input closed, exiting)")
            running = false
            continue

        // empty line: just redraw the menu
        if raw == ""
            continue

        // normalize so "ADD" and "add" are equivalent
        choice = string.lower(raw)

        match choice
            case "add"
                title = prompt("Title:")
                if title == none
                    running = false
                    continue
                pr = prompt("Priority [low/normal/high]:")
                if pr == none
                    running = false
                    continue
                add_task(title, string.lower(pr))
            case "list"
                f = prompt("Filter [all/todo/done]:")
                if f == none
                    running = false
                    continue
                list_tasks(string.lower(f))
            case "done"
                raw_id = prompt("Task id:")
                if raw_id == none
                    running = false
                    continue
                num = number(raw_id)
                if num == none
                    os.output("Not a number.")
                else
                    complete_task(num)
            case "del"
                raw_id = prompt("Task id:")
                if raw_id == none
                    running = false
                    continue
                num = number(raw_id)
                if num == none
                    os.output("Not a number.")
                else
                    delete_task(num)
            case "stats"
                stats()
            case "save"
                save_tasks()
            case "export"
                count = await export_report()
                os.output("Exported {count} task(s) to report.md.")
            case "quit"
                running = false
            case
                os.output("Unknown command: '{choice}'.")

// bootstrap: print banner, load or seed the store, run the loop,
// then persist whatever state remains
async function main()
    os.output("=== Apex Task Tracker ===")
    os.output("Platform: {sys.platform()} {sys.architecture()} | Apex {sys.apex_version()}")
    load_tasks()

    // first run: seed with an example task so the user has
    // something to look at immediately
    if table.size(tasks) == 0
        add_task("Read the reference manual", "normal")

    await main_loop()

    // always save, even if the user quit without typing "save"
    save_tasks()
    os.output("Bye.")

// top-level await is allowed at the program root
await main()