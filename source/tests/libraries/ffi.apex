import ffi
import os
import sys

function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function load_library()
    lib_name = "source/tests/libraries/libtest.so"
    platform = sys.platform()
    if platform == "Windows"
        lib_name = "source/tests/libraries/libtest.dll"
    
    lib = ffi.open(lib_name)
    assert_equal(lib != none, true, "ffi.open valid library")
    if lib != none
        return lib
    
    return none

function add_positive(lib)
    if lib == none
        return
    res = ffi.call(lib, "add", 10, 20)
    assert_equal(res, 30, "ffi.call add 10+20")

function add_negative(lib)
    if lib == none
        return
    res = ffi.call(lib, "add", -5, 5)
    assert_equal(res, 0, "ffi.call add -5+5")

function malloc_free()
    ptr = ffi.malloc(64)
    assert_equal(ptr != none, true, "ffi.malloc returns pointer")
    if ptr != none
        ffi.free(ptr)
        assert_equal(true, true, "ffi.free executed")

function errno()
    err_code = ffi.errno()
    assert_equal(err_code != none, true, "ffi.errno returns value")
    if err_code != none
        msg = ffi.strerror(err_code)
        assert_equal(msg != none, true, "ffi.strerror returns string")

function non_existent_function(lib)
    if lib == none
        return
    bad_res = ffi.call(lib, "non_existent_function", 1)
    assert_equal(bad_res, none, "ffi.call non-existent function")

function invalid_library()
    bad_lib = ffi.open("lib_that_does_not_exist.so")
    assert_equal(bad_lib, none, "ffi.open invalid library")

lib = load_library()
add_positive(lib)
add_negative(lib)
malloc_free()
errno()
non_existent_function(lib)
invalid_library()

os.output("[PASS] libraries/ffi.apex")