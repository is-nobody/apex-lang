// test_imports.apex - Fixed version
import os

// Test 1: Import entire module
import math_lib
os.output("=== Test 1: Import entire module ===")
os.output("Pi: {math_lib.pi}")

// Store function results in variables first
add_result = math_lib.add(5, 3)
os.output("Add: {add_result}")

circle_result = math_lib.circle_area(2)
os.output("Circle area: {circle_result}")

// Test 2: Import specific members
import math_lib: e, multiply, factorial
os.output("")
os.output("=== Test 2: Import specific members ===")
os.output("E: {e}")

mult_result = multiply(4, 5)
os.output("Multiply: {mult_result}")

fact_result = factorial(6)
os.output("Factorial: {fact_result}")

// Test 3: Mix imported and module access
import math_lib: pi, add
os.output("")
os.output("=== Test 3: Mix imported and module access ===")
os.output("Pi direct: {pi}")

add_direct = add(10, 20)
os.output("Add direct: {add_direct}")

os.output("Pi from module: {math_lib.pi}")

sub_result = math_lib.subtract(15, 7)
os.output("Subtract from module: {sub_result}")

// Test 4: Import same module twice (should use cache)
import math_lib
os.output("")
os.output("=== Test 4: Duplicate imports ===")
os.output("Second import successful")

power_result = math_lib.power(2, 8)
os.output("Power: {power_result}")

// Test 5: Import with table access
colors = (
    red = math_lib.pi,
    blue = e,
    green = math_lib.add(2, 2)
)
os.output("")
os.output("=== Test 5: Table with imported values ===")
os.output("Red value: {colors.red}")
os.output("Blue value: {colors.blue}")
os.output("Green value: {colors.green}")