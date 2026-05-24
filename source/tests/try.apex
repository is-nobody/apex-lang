import os

errors = 0

// division by zero
try
    x = 10 / 0
failure
    if error != "division_by_zero"
        errors = errors + 1
        os.output("  ERROR: division by zero test failed, got {error}, expected division_by_zero")
always
    // silent

// name error
try
    x = undefined_variable
failure
    if error != "name_error"
        errors = errors + 1
        os.output("  ERROR: name error test failed, got {error}, expected name_error")
always
    // silent

// custom error with os.read
try
    content = os.read("nonexistent_file.xyz")
    x = 10 / 0
failure
    // silent - just checking it doesn't crash
always
    // silent

// conditional error handling
try
    x = 10 / 0
failure
    if error != "division_by_zero"
        errors = errors + 1
        os.output("  ERROR: conditional handling test failed, got {error}, expected division_by_zero")
always
    // silent

// nested try blocks
try
    try
        x = 10 / 0
    failure
        if error != "division_by_zero"
            errors = errors + 1
            os.output("  ERROR: nested try inner test failed, got {error}, expected division_by_zero")
    always
        // silent
failure
    errors = errors + 1
    os.output("  ERROR: nested try outer test failed, outer should not catch")
always
    // silent

// no error (should not enter failure)
try
    x = 10 + 5
failure
    errors = errors + 1
    os.output("  ERROR: no error test failed, failure block ran when it shouldn't")
always
    // silent

// final summary
os.output("Total errors in try.apex: {errors}")

if errors == 0
    os.output("All tests in try.apex passed!")
else
    os.output("{errors} test(s) failed in try.apex.")