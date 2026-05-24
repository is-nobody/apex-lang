import os

os.output("Testing Try-Failure with All Error Types")
os.output("")

// 1. Division by zero
os.output("1. Testing division by zero:")
try
    x = 10 / 0
failure
    os.output("Error: {error}")
    if error == "division_by_zero"
        os.output("OK: division_by_zero")
    else
        os.output("FAIL: wrong error type")
always
    os.output("Always block executed")
os.output("")

// 2. Name error
os.output("2. Testing name error:")
try
    x = undefined_variable
failure
    os.output("Error: {error}")
    if error == "name_error"
        os.output("OK: name_error")
    else
        os.output("FAIL: wrong error type")
always
    os.output("Always block executed")
os.output("")

// 3. Custom error with os.read
os.output("3. Testing with os.read:")
try
    content = os.read("nonexistent_file.xyz")
    x = 10 / 0
failure
    os.output("Error: {error}")
always
    os.output("Always block executed")
os.output("")

// 4. Conditional error handling
os.output("4. Testing conditional handling:")
try
    x = 10 / 0
failure
    if error == "division_by_zero"
        os.output("Handling division by zero")
        x = 0
    else
        os.output("Other error: {error}")
always
    os.output("Cleanup completed")
os.output("")

// 5. Nested try blocks
os.output("5. Testing nested try blocks:")
try
    os.output("Outer try block")
    try
        x = 10 / 0
    failure
        os.output("Inner error: {error}")
    always
        os.output("Inner always")
failure
    os.output("Outer error: {error}")
always
    os.output("Outer always")
os.output("")

os.output("All tests completed!")