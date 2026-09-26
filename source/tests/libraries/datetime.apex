// source/tests/libraries/datetime.apex
// Implementation of Datetime Library Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import datetime
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: verifies a boolean condition
function assert_true(cond, name)
    if cond != true
        os.output("[ERROR] {name}: condition is not true")

// test: now() returns current UTC date/time as structured table
function test_now()
    d = datetime.now()
    is_table = type(d) == "table"
    assert_true(is_table, "now returns table")
    if is_table == true
        year_valid        = d["year"] >= 2024 and d["year"] <= 2100
        month_valid       = d["month"] >= 1 and d["month"] <= 12
        day_valid         = d["day"] >= 1 and d["day"] <= 31
        hour_valid        = d["hour"] >= 0 and d["hour"] <= 23
        minute_valid      = d["minute"] >= 0 and d["minute"] <= 59
        second_valid      = d["second"] >= 0 and d["second"] <= 59
        millisecond_valid = d["millisecond"] >= 0 and d["millisecond"] <= 999
        weekday_valid     = d["weekday"] >= 0 and d["weekday"] <= 6

        assert_true(year_valid,        "now year in range")
        assert_true(month_valid,       "now month in range")
        assert_true(day_valid,         "now day in range")
        assert_true(hour_valid,        "now hour in range")
        assert_true(minute_valid,      "now minute in range")
        assert_true(second_valid,      "now second in range")
        assert_true(millisecond_valid, "now millisecond in range")
        assert_true(weekday_valid,     "now weekday in range")

// test: local() returns current local date/time as structured table
function test_local()
    d = datetime.local()
    is_table = type(d) == "table"
    assert_true(is_table, "local returns table")
    if is_table == true
        year_valid   = d["year"] >= 2024 and d["year"] <= 2100
        month_valid  = d["month"] >= 1 and d["month"] <= 12
        day_valid    = d["day"] >= 1 and d["day"] <= 31
        hour_valid   = d["hour"] >= 0 and d["hour"] <= 23
        weekday_valid = d["weekday"] >= 0 and d["weekday"] <= 6
        assert_true(year_valid,    "local year in range")
        assert_true(month_valid,   "local month in range")
        assert_true(day_valid,     "local day in range")
        assert_true(hour_valid,    "local hour in range")
        assert_true(weekday_valid, "local weekday in range")

// test: timestamp() returns seconds since epoch with microsecond precision
function test_timestamp()
    t = datetime.timestamp()
    is_number = type(t) == "number"
    assert_true(is_number, "timestamp returns number")
    if is_number == true
        // 2024-01-01 == 1704067200; 2100-01-01 == 4102444800
        after_2024  = t >= 1704067200.0
        before_2100 = t <= 4102444800.0
        assert_true(after_2024,  "timestamp after 2024-01-01")
        assert_true(before_2100, "timestamp before 2100-01-01")

    // two consecutive calls should be non-decreasing
    t1 = datetime.timestamp()
    t2 = datetime.timestamp()
    assert_true(t2 >= t1, "timestamp monotonic within run")

// test: from_timestamp converts epoch seconds to UTC table
function test_from_timestamp()
    d = datetime.from_timestamp(0)
    assert_true(type(d) == "table", "from_timestamp returns table")
    assert_equal(d["year"],    1970, "from_timestamp(0) year")
    assert_equal(d["month"],   1,    "from_timestamp(0) month")
    assert_equal(d["day"],     1,    "from_timestamp(0) day")
    assert_equal(d["hour"],    0,    "from_timestamp(0) hour")
    assert_equal(d["minute"],  0,    "from_timestamp(0) minute")
    assert_equal(d["second"],  0,    "from_timestamp(0) second")
    assert_equal(d["weekday"], 4,    "from_timestamp(0) weekday (thursday)")

    d2 = datetime.from_timestamp(1704067200)          // 2024-01-01 00:00:00 UTC
    assert_equal(d2["year"],    2024, "from_timestamp(1704067200) year")
    assert_equal(d2["month"],   1,    "from_timestamp(1704067200) month")
    assert_equal(d2["day"],     1,    "from_timestamp(1704067200) day")
    assert_equal(d2["weekday"], 1,    "from_timestamp(1704067200) weekday (monday)")

    // leap year day: 2024-02-29 00:00:00 UTC == 1709164800
    d3 = datetime.from_timestamp(1709164800)
    assert_equal(d3["year"],  2024, "from_timestamp leap year")
    assert_equal(d3["month"], 2,    "from_timestamp leap month")
    assert_equal(d3["day"],   29,   "from_timestamp leap day")

// test: to_timestamp converts table back to epoch seconds
function test_to_timestamp()
    epoch = ["year" = 1970, "month" = 1, "day" = 1,
             "hour" = 0, "minute" = 0, "second" = 0, "millisecond" = 0]
    assert_equal(datetime.to_timestamp(epoch), 0, "to_timestamp(epoch) == 0")

    y2024 = ["year" = 2024, "month" = 1, "day" = 1,
             "hour" = 0, "minute" = 0, "second" = 0, "millisecond" = 0]
    assert_equal(datetime.to_timestamp(y2024), 1704067200, "to_timestamp(2024-01-01)")

    // round-trip: from_timestamp -> to_timestamp should be identity
    ts = 1718450000.0
    d = datetime.from_timestamp(ts)
    back = datetime.to_timestamp(d)
    assert_equal(back, ts, "to_timestamp round-trip")

    // missing time fields should default to 0
    date_only = ["year" = 2000, "month" = 6, "day" = 15]
    assert_equal(datetime.to_timestamp(date_only), 961027200, "to_timestamp date-only")

// test: parse handles ISO-8601 in its supported forms
function test_parse()
    d1 = datetime.parse("2024-06-15")
    assert_equal(d1["year"],    2024, "parse date-only year")
    assert_equal(d1["month"],   6,    "parse date-only month")
    assert_equal(d1["day"],     15,   "parse date-only day")
    assert_equal(d1["hour"],    0,    "parse date-only hour default")
    assert_equal(d1["weekday"], 6,    "parse 2024-06-15 weekday (saturday)")

    d2 = datetime.parse("2024-06-15 14:30:00")
    assert_equal(d2["hour"],   14, "parse space separator hour")
    assert_equal(d2["minute"], 30, "parse space separator minute")
    assert_equal(d2["second"], 0,  "parse space separator second")

    d3 = datetime.parse("2024-06-15T14:30:45")
    assert_equal(d3["hour"],   14, "parse T separator hour")
    assert_equal(d3["minute"], 30, "parse T separator minute")
    assert_equal(d3["second"], 45, "parse T separator second")

    d4 = datetime.parse("2024-06-15 14:30:45.250")
    assert_equal(d4["second"],      45,  "parse fractional second")
    assert_equal(d4["millisecond"], 250, "parse millisecond")

    d5 = datetime.parse("2024-02-29")               // leap day accepted
    assert_equal(d5["day"], 29, "parse leap day")

    // invalid inputs return none
    assert_equal(datetime.parse("not a date"),     none, "parse garbage")
    assert_equal(datetime.parse("2024/06/15"),     none, "parse wrong separators")
    assert_equal(datetime.parse("2024-13-01"),     none, "parse month out of range")
    assert_equal(datetime.parse("2024-06-32"),     none, "parse day out of range")
    assert_equal(datetime.parse("2023-02-29"),     none, "parse non-leap feb 29")
    assert_equal(datetime.parse("2024-06-15T25"), none, "parse malformed time")

// test: format renders strftime-like tokens
function test_format()
    d = ["year" = 2024, "month" = 6, "day" = 15,
         "hour" = 14, "minute" = 30, "second" = 45, "millisecond" = 250]

    assert_equal(datetime.format(d, "%Y-%m-%d"),              "2024-06-15",              "format iso date")
    assert_equal(datetime.format(d, "%H:%M:%S"),              "14:30:45",                "format iso time")
    assert_equal(datetime.format(d, "%Y-%m-%d %H:%M:%S.%f"),  "2024-06-15 14:30:45.250", "format iso with ms")
    assert_equal(datetime.format(d, "%A"),                    "Saturday",                "format long weekday")
    assert_equal(datetime.format(d, "%a"),                    "Sat",                     "format short weekday")
    assert_equal(datetime.format(d, "%B"),                    "June",                    "format long month")
    assert_equal(datetime.format(d, "%b"),                    "Jun",                     "format short month")
    assert_equal(datetime.format(d, "%j"),                    "167",                     "format day of year")
    assert_equal(datetime.format(d, "100%%"),                 "100%",                    "format literal percent")
    assert_equal(datetime.format(d, "no tokens"),             "no tokens",               "format plain text")

    // invalid inputs return none
    assert_equal(datetime.format(d, 42),                 none, "format non-string template")
    assert_equal(datetime.format(["day" = 1], "%Y"),     none, "format missing year")

// test: add shifts by a unit amount
function test_add()
    base = ["year" = 2024, "month" = 1, "day" = 31,
            "hour" = 12, "minute" = 0, "second" = 0, "millisecond" = 0]

    // month arithmetic with day clamp: 2024-01-31 + 1 month = 2024-02-29
    m1 = datetime.add(base, 1, "month")
    assert_equal(m1["year"],  2024, "add month year")
    assert_equal(m1["month"], 2,    "add month month")
    assert_equal(m1["day"],   29,   "add month day clamped to leap feb")

    // 2023-01-31 + 1 month = 2023-02-28
    base_2023 = ["year" = 2023, "month" = 1, "day" = 31]
    m2 = datetime.add(base_2023, 1, "month")
    assert_equal(m2["day"], 28, "add month day clamped to non-leap feb")

    // year arithmetic with day clamp: 2024-02-29 + 1 year = 2025-02-28
    leap = ["year" = 2024, "month" = 2, "day" = 29]
    y1 = datetime.add(leap, 1, "year")
    assert_equal(y1["year"],  2025, "add year year")
    assert_equal(y1["month"], 2,    "add year month")
    assert_equal(y1["day"],   28,   "add year day clamped")

    // day arithmetic across a month boundary
    d1 = datetime.add(base, 1, "day")
    assert_equal(d1["month"], 2, "add day crosses month")
    assert_equal(d1["day"],   1, "add day value")

    // week arithmetic
    w1 = datetime.add(base, 1, "week")
    assert_equal(w1["month"], 2, "add week crosses month")
    assert_equal(w1["day"],   7, "add week value")

    // hour arithmetic across midnight
    h1 = datetime.add(base, 13, "hour")
    assert_equal(h1["day"],  1, "add hour day rolls forward")
    assert_equal(h1["hour"], 1, "add hour value")

    // negative amounts
    back = datetime.add(base, -1, "day")
    assert_equal(back["year"],  2024, "add negative year")
    assert_equal(back["month"], 1,    "add negative month")
    assert_equal(back["day"],   30,   "add negative day")

    // unknown unit
    assert_equal(datetime.add(base, 1, "fortnight"), none, "add unknown unit")

// test: diff computes the gap between two datetimes in a unit
function test_diff()
    a = ["year" = 2024, "month" = 6, "day" = 15,
         "hour" = 12, "minute" = 0, "second" = 0, "millisecond" = 0]
    b = ["year" = 2024, "month" = 6, "day" = 14,
         "hour" = 12, "minute" = 0, "second" = 0, "millisecond" = 0]

    assert_equal(datetime.diff(a, b, "day"),    1,     "diff day")
    assert_equal(datetime.diff(a, b, "hour"),   24,    "diff hour")
    assert_equal(datetime.diff(a, b, "minute"), 1440,  "diff minute")
    assert_equal(datetime.diff(a, b, "second"), 86400, "diff second")
    assert_equal(datetime.diff(b, a, "day"),    -1,    "diff negative")

    // week boundary: 2024-06-15 is a saturday, 2024-06-08 a saturday
    a2 = ["year" = 2024, "month" = 6, "day" = 15]
    b2 = ["year" = 2024, "month" = 6, "day" = 8]
    assert_equal(datetime.diff(a2, b2, "week"), 1, "diff whole week")

    // month / year units are not supported; expect none
    assert_equal(datetime.diff(a, b, "month"), none, "diff rejects month")
    assert_equal(datetime.diff(a, b, "year"),  none, "diff rejects year")

    // unknown unit
    assert_equal(datetime.diff(a, b, "fortnight"), none, "diff unknown unit")

// test: cross-check parsing + arithmetic + formatting together
function test_roundtrip()
    parsed  = datetime.parse("2000-01-01 00:00:00")
    shifted = datetime.add(parsed, 1, "year")
    out     = datetime.format(shifted, "%Y-%m-%d")
    assert_equal(out, "2001-01-01", "roundtrip parse+add+format")

    // to_timestamp and from_timestamp should be inverses
    original = ["year" = 1985, "month" = 6, "day" = 8,
                "hour" = 0, "minute" = 0, "second" = 0, "millisecond" = 0]
    ts   = datetime.to_timestamp(original)
    back = datetime.from_timestamp(ts)
    assert_equal(back["year"],  1985, "roundtrip year")
    assert_equal(back["month"], 6,    "roundtrip month")
    assert_equal(back["day"],   8,    "roundtrip day")

// execute all datetime test cases
test_now()
test_local()
test_timestamp()
test_from_timestamp()
test_to_timestamp()
test_parse()
test_format()
test_add()
test_diff()
test_roundtrip()

// report completion of datetime library tests
os.output("[PASS] libraries/datetime.apex")