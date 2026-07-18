import os
import sys
import math
import table

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function apex_version()
    version = sys.apex_version()
    is_string = type(version) == "string"
    assert_equal(is_string, true, "apex_version returns string")

function architecture()
    arch = sys.architecture()
    is_string = type(arch) == "string"
    assert_equal(is_string, true, "architecture returns string")

function disksize()
    disk = sys.disksize()
    is_table = type(disk) == "table"
    assert_equal(is_table, true, "disksize returns table")
    if is_table == true
        has_total = disk["total"] != false
        has_free = disk["free"] != false
        has_used = disk["used"] != false
        assert_equal(has_total, true, "disk has total")
        assert_equal(has_free, true, "disk has free")
        assert_equal(has_used, true, "disk has used")

function environment()
    e = sys.environment()
    is_table = type(e) == "table"
    assert_equal(is_table, true, "environment returns table")
    if is_table == true
        p = sys.platform()
        if p == "Windows"
            has_user = table.has(e, "USERNAME")
            has_path = table.has(e, "Path")
            has_home = table.has(e, "USERPROFILE")
            assert_equal(has_user, true, "environment has USERNAME")
            assert_equal(has_path, true, "environment has PATH")
            assert_equal(has_home, true, "environment has HOME/USERPROFILE")
        else
            has_user = table.has(e, "USER")
            has_path = table.has(e, "PATH")
            has_home = table.has(e, "HOME")
            assert_equal(has_user, true, "environment has USER")
            assert_equal(has_path, true, "environment has PATH")
            assert_equal(has_home, true, "environment has HOME")

function executable()
    exe = sys.executable()
    is_string = type(exe) == "string"
    assert_equal(is_string, true, "executable returns string")

function homedir()
    dir = sys.homedir()
    is_string = type(dir) == "string"
    assert_equal(is_string, true, "homedir returns string")

function hostname()
    host = sys.hostname()
    is_string = type(host) == "string"
    assert_equal(is_string, true, "hostname returns string")

function isterminal()
    isterm = sys.isterminal()
    assert_equal(isterm, true, "isterminal returns true")

function platform()
    p = sys.platform()
    is_string = type(p) == "string"
    assert_equal(is_string, true, "platform returns string")

function process_id()
    p = sys.process_id()
    is_number = type(p) == "number"
    assert_equal(is_number, true, "process_id returns number")

function tempdir()
    temp_dir = sys.tempdir()
    is_string = type(temp_dir) == "string"
    assert_equal(is_string, true, "tempdir returns string")

function user()
    u = sys.user()
    is_string = type(u) == "string"
    assert_equal(is_string, true, "user returns string")

function time()
    t = sys.time()
    assert_equal(type(t) == "number", true, "time returns number")

function date()
    d = sys.date()
    is_table = type(d) == "table"
    assert_equal(is_table, true, "date returns table")
    if is_table == true
        year_valid = d["year"] >= 2024 and d["year"] <= 2100
        month_valid = d["month"] >= 1 and d["month"] <= 12
        week_valid = d["week"] >= 0 and d["week"] <= 6
        day_valid = d["day"] >= 1 and d["day"] <= 31
        hour_valid = d["hour"] >= 0 and d["hour"] <= 23
        minute_valid = d["minute"] >= 0 and d["minute"] <= 59
        second_valid = d["second"] >= 0 and d["second"] <= 59
        millisecond_valid = d["millisecond"] >= 0 and d["millisecond"] <= 999
        
        assert_equal(year_valid, true, "date year in range")
        assert_equal(month_valid, true, "date month in range")
        assert_equal(week_valid, true, "date week in range")
        assert_equal(day_valid, true, "date day in range")
        assert_equal(hour_valid, true, "date hour in range")
        assert_equal(minute_valid, true, "date minute in range")
        assert_equal(second_valid, true, "date second in range")
        assert_equal(millisecond_valid, true, "date millisecond in range")

apex_version()
architecture()
disksize()
environment()
executable()
homedir()
hostname()
isterminal()
platform()
process_id()
tempdir()
user()
time()
date()

if tests_failed == 0
    os.output("[OK] libraries/sys.apex")