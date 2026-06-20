import os
import math
import lib

add_result = lib.add(5, 3)
circle_result = lib.circle_area(2)

os.output("Import lib.pi expected {math.pi()}, got {lib.pi}")
os.output("lib.add(5,3), expected 8, got {add_result}")
os.output("lib.circle_area(2), expected 12.5663706143592, got {circle_result}")

power_result = lib.power(2, 8)
os.output("lib.power(2,8), expected 256, got {power_result}")

colors = (
    red = lib.pi,
    blue = lib.e,
    green = lib.add(2, 2)
)

os.output("Table colors.red expected {math.pi()}, got {colors.red}")
os.output("Table colors.blue expected {math.e()}, got {colors.blue}")
os.output("Table colors.green expected 4, got {colors.green}")