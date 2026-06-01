// imports.apex
import os


// import entire module
import math_lib

// store function results in variables first
add_result = math_lib.add(5, 3)
circle_result = math_lib.circle_area(2)

os.output("Import math_lib.pi expected 3.14159, got {math_lib.pi}")

os.output("math_lib.add(5,3), expected 8, got {add_result}")

os.output("math_lib.circle_area(2), expected 12.56636, got {circle_result}")

// import same module twice (should use cache)
import math_lib

power_result = math_lib.power(2, 8)

os.output("math_lib.power(2,8), expected 256, got {power_result}")

// import with table access
colors = (
    red = math_lib.pi,
    blue = math_lib.e,
    green = math_lib.add(2, 2)
)

os.output("Table colors.red expected 3.14159, got {colors.red}")

os.output("Table colors.blue expected 2.71828, got {colors.blue}")

os.output("Table colors.green expected 4, got {colors.green}")

// final result
