import os
import math
import lib

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

add_result = lib.add(5, 3)
circle_result = lib.circle_area(2)
power_result = lib.power(2, 8)

assert_equal(lib.pi, math.pi(), "lib.pi")
assert_equal(add_result, 8, "lib.add(5, 3)")
assert_equal(circle_result, math.pi() * 2 * 2, "lib.circle_area(2)")
assert_equal(power_result, 256, "lib.power(2, 8)")

colors = [
    "red" = lib.pi,
    "blue" = lib.e,
    "green" = lib.add(2, 2)
]

assert_equal(colors["red"], math.pi(), "colors[\"red\"]")
assert_equal(colors["blue"], math.e(), "colors[\"blue\"]")
assert_equal(colors["green"], 4, "colors[\"green\"]")

if tests_failed == 0
    os.output("[OK] integration/import.apex")