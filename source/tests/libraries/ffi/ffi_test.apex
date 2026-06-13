import ffi
import os

function check(name, actual, expected)
    if actual == expected
        os.output("[OK] {name}")
    else
        os.output("[FAIL] {name}: got {actual}, expected {expected}")

function load_library(lib_name)
    os.output("Loading library: {lib_name}")
    
    lib = ffi.open(lib_name)
    if lib == false
        os.output("[FAIL] Could not open library '{lib_name}'")
        os.exit(1)
    
    os.output("[OK] Library loaded successfully")
    return lib

function test_add_positive(lib)
    res = ffi.call(lib, "add", 10, 20)
    check("ffi.call(add, 10, 20)", res, 30)

function test_add_negative(lib)
    res_neg = ffi.call(lib, "add", -5, 5)
    check("ffi.call(add, -5, 5)", res_neg, 0)

function test_malloc_free()
    ptr = ffi.malloc(64)
    if ptr > 0
        os.output("[OK] ffi.malloc returned valid pointer: {ptr}")
        ffi.free(ptr)
        os.output("[OK] ffi.free executed without crash")
    else
        os.output("[FAIL] ffi.malloc failed")

function test_errno()
    err_code = ffi.errno()
    if err_code != false
        os.output("[OK] ffi.errno returned: {err_code}")
        msg = ffi.strerror(err_code)
        os.output("[OK] ffi.strerror({err_code}): {msg}")
    else
        os.output("[FAIL] ffi.errno failed")

function test_non_existent_function(lib)
    bad_res = ffi.call(lib, "non_existent_function", 1)
    check("ffi.call(non_existent)", bad_res, false)

function test_invalid_library()
    bad_lib = ffi.open("lib_that_does_not_exist.so")
    check("ffi.open(invalid_path)", bad_lib, false)

lib_name = "source/tests/libraries/ffi/libtest.so"
if os.platform() == "Windows"
    lib_name = "source/tests/libraries/ffi/libtest.dll"

lib = load_library(lib_name)
test_add_positive(lib)
test_add_negative(lib)
test_malloc_free()
test_errno()
test_non_existent_function(lib)
test_invalid_library()