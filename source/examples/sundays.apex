// source/examples/sundays.apex
// Implementation of the Sundays Visualizer on Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os       // args, environment, output, print, exit
import sys      // environment() for the fallback username
import math     // round_down() to turn fractional days into whole weeks
import string   // repeat() for the right-aligned last-year label
import datetime // parse(), local(), diff()

// ansi escape codes via octal escapes (\033 = ESC)
constant C_RESET = "\033[0m"               // clears all styling
constant C_PAST  = "\033[41m  \033[0m "    // red block: year already lived
constant C_LEFT  = "\033[42m  \033[0m "    // green block: year still ahead

// how many years a person is expected to live; matches the original script
constant LIFE_EXPECTANCY = 80

// how many year-squares fit on one row
constant COLUMNS = 20

// indentation used to right-align the final year under the grid
constant LAST_YEAR_INDENT = 55

// entry point
args = os.args()
birthdate_arg = args[1]
if birthdate_arg == none
    os.output("Usage: apex sundays.apex birthdate [username]")
    os.output("Example: apex sundays.apex 2000-01-01 Alice")
    os.exit(1)

name = args[2]
if name == none
    name = sys.environment["USER"]
    if name == none
        name = "friend"

birth = datetime.parse(birthdate_arg)
if birth == none
    os.output("Invalid date format. Please use YYYY-MM-DD.")
    os.exit(1)

now = datetime.local()

// days_passed is a float; dividing by 7 and rounding down gives whole weeks
days_passed     = datetime.diff(now, birth, "day")
weeks_passed    = math.round_down(days_passed / 7)
weeks_remaining = LIFE_EXPECTANCY * 52 - weeks_passed

birth_year   = birth["year"]
years_passed = now["year"] - birth_year
last_year    = birth_year + LIFE_EXPECTANCY - 1

// headline
os.print("{name}, only {weeks_remaining} Sundays remain\n\n")

// grid: one colored square per year of expected life
for year_index = 0, LIFE_EXPECTANCY - 1
    if year_index == 0
        os.print("{birth_year}\n")         // birth year above the first square
    if year_index < years_passed
        os.print(C_PAST)                   // already lived: red
    else
        os.print(C_LEFT)                   // remaining: green
    if (year_index + 1) % COLUMNS == 0
        if year_index == LIFE_EXPECTANCY - 1
            os.print("\n")                 // last row: single newline
        else
            os.print("\n\n")               // blank line between rows

// right-aligned last-year label under the final column
indent = string.repeat(" ", LAST_YEAR_INDENT)
os.print("{indent}{last_year}")

os.print("\n\nHow are you going to spend these Sundays, {name}?\n")