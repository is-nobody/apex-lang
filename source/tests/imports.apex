// imports.apex
import os

errors = 0

// import entire module
import math_lib

// store function results in variables first
add_result = math_lib.add(5, 3)
circle_result = math_lib.circle_area(2)

if math_lib.pi != 3.14159
    os.output("  ERROR: Import math_lib.pi expected 3.14159, got {math_lib.pi}")
    errors = errors + 1

if add_result != 8
    os.output("  ERROR: math_lib.add(5,3) expected 8, got {add_result}")
    errors = errors + 1

if circle_result != 12.56636
    os.output("  ERROR: math_lib.circle_area(2) expected 12.56636, got {circle_result}")
    errors = errors + 1

// import same module twice (should use cache)
import math_lib

power_result = math_lib.power(2, 8)

if power_result != 256
    os.output("  ERROR: math_lib.power(2,8) expected 256, got {power_result}")
    errors = errors + 1

// import with table access
colors = (
    red = math_lib.pi,
    blue = math_lib.e,
    green = math_lib.add(2, 2)
)

if colors.red != 3.14159
    os.output("  ERROR: Table colors.red expected 3.14159, got {colors.red}")
    errors = errors + 1

if colors.blue != 2.71828
    os.output("  ERROR: Table colors.blue expected 2.71828, got {colors.blue}")
    errors = errors + 1

if colors.green != 4
    os.output("  ERROR: Table colors.green expected 4, got {colors.green}")
    errors = errors + 1

// final result
os.output("Total errors in test_imports.apex: {errors}")

if errors == 0
    os.output("Tests in test_imports.apex passed!")
else
    os.output("{errors} test(s) failed in test_imports.apex.")