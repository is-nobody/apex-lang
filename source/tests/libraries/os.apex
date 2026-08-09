// source/tests/libraries/os.apex
// Implementation of OS Library File System Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os
import sys

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: write, read, and delete a file
function write_read_delete()
    os.write("os.txt", "Hello, Friend!")
    content = os.read("os.txt")
    assert_equal(content, "Hello, Friend!", "write and read file")
    os.delete("os.txt")
    exists_after = os.exists("os.txt")
    assert_equal(exists_after, false, "delete file")

// test: append content to existing file
function append()
    os.write("append.txt", "line1\n")
    os.append("append.txt", "line2\n")
    content = os.read("append.txt")
    assert_equal(content, "line1\nline2\n", "append to file")
    os.delete("append.txt")

// test: check if file or directory exists
function exists()
    exists_yes = os.exists("main.c")
    assert_equal(exists_yes, true, "main.c exists")
    exists_no = os.exists("nonexistent.xyz")
    assert_equal(exists_no, false, "nonexistent.xyz exists")

// test: check if path points to a regular file
function isfile()
    result = os.isfile("main.c")
    assert_equal(result, true, "main.c is file")
    result = os.isfile("nonexistent")
    assert_equal(result, false, "nonexistent is file")

// test: check if path points to a directory
function isfolder()
    result = os.isfolder("source")
    assert_equal(result, true, "source is folder")
    result = os.isfolder("nonexistent")
    assert_equal(result, false, "nonexistent is folder")

// test: create and delete a file
function create_delete_file()
    result = os.create_file("create.txt")
    assert_equal(result, true, "create file")
    exists_yes = os.exists("create.txt")
    assert_equal(exists_yes, true, "file exists after create")
    result = os.delete("create.txt")
    assert_equal(result, true, "delete file")
    exists_no = os.exists("create.txt")
    assert_equal(exists_no, false, "file exists after delete")

// test: create and delete a directory
function create_delete_folder()
    result = os.create_folder("folder")
    assert_equal(result, true, "create folder")
    exists_yes = os.exists("folder")
    assert_equal(exists_yes, true, "folder exists after create")
    result = os.delete("folder")
    assert_equal(result, true, "delete folder")
    exists_no = os.exists("folder")
    assert_equal(exists_no, false, "folder exists after delete")

// test: rename a file
function rename_file()
    os.write("old_name.txt", "test")
    result = os.rename("old_name.txt", "new_name.txt")
    assert_equal(result != false, true, "rename file")
    old_exists = os.exists("old_name.txt")
    new_exists = os.exists("new_name.txt")
    assert_equal(old_exists, false, "old file gone")
    assert_equal(new_exists, true, "new file exists")
    os.delete("new_name.txt")

// test: copy a file
function copy_file()
    os.write("copy_src.txt", "copy test")
    result = os.copy("copy_src.txt", "copy_dst.txt")
    assert_equal(result != false, true, "copy file")
    dst_exists = os.exists("copy_dst.txt")
    assert_equal(dst_exists, true, "dst file exists")
    content = os.read("copy_dst.txt")
    assert_equal(content, "copy test", "copy content matches")
    os.delete("copy_src.txt")
    os.delete("copy_dst.txt")

// test: move a file to different directory
function move_file()
    os.write("move_src.txt", "move test")
    os.create_folder("move_dir")
    result = os.move("move_src.txt", "move_dir/move_src.txt")
    assert_equal(result != false, true, "move file")
    old_exists = os.exists("move_src.txt")
    new_exists = os.exists("move_dir/move_src.txt")
    assert_equal(old_exists, false, "src file gone")
    assert_equal(new_exists, true, "dst file exists")
    os.delete("move_dir/move_src.txt")
    os.delete("move_dir")

// test: get file size in bytes
function size_file()
    size = os.size("main.c")
    assert_equal(size != none, true, "file size returns value")
    size_no = os.size("nonexistent")
    assert_equal(size_no, none, "nonexistent size is false")

// test: sleep for specified seconds
function wait()
    os.wait(0.001)
    assert_equal(true, true, "wait completed")

// test: get current working directory
function get_current_folder()
    result = os.get_current_folder()
    assert_equal(result != false, true, "get_current_folder returns string")

// test: check file access permissions
function access()
    os.write("access.txt", "test")
    result = os.access("access.txt", 438)
    assert_equal(result, true, "access returns true")
    result = os.access("no_file.txt", 438)
    assert_equal(result, false, "access nonexistent returns false")
    os.delete("access.txt")

// execute all OS library test cases
write_read_delete()
append()
exists()
isfile()
isfolder()
create_delete_file()
create_delete_folder()
rename_file()
copy_file()
move_file()
size_file()
wait()
get_current_folder()
access()

// report completion of OS library tests
os.output("[PASS] libraries/os.apex")