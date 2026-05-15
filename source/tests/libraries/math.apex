import math
import os

os.output("")
os.output("--- math.abs() ---")
os.output(math.abs(-5))
os.output(math.abs(3.14))

os.output("")
os.output("--- math.floor() ---")
os.output(math.floor(3.7))
os.output(math.floor(3.2))
os.output(math.floor(-3.7))

os.output("")
os.output("--- math.ceil() ---")
os.output(math.ceil(3.7))
os.output(math.ceil(3.2))
os.output(math.ceil(-3.7))

os.output("")
os.output("--- math.round() ---")
os.output(math.round(3.14159))
os.output(math.round(3.14159, 2))
os.output(math.round(3.14159, 4))

os.output("")
os.output("--- math.sqrt() ---")
os.output(math.sqrt(16))
os.output(math.sqrt(2))
os.output(math.sqrt(0))

os.output("")
os.output("--- math.exp() ---")
os.output(math.exp(1))
os.output(math.exp(0))

os.output("")
os.output("--- math.log() ---")
os.output(math.log(2.71828))
os.output(math.log(100, 10))

os.output("")
os.output("--- math.sin/cos/tan ---")
angle = 30 * 3.14 / 180
os.output(math.sin(angle))
os.output(math.cos(60 * 3.14 / 180))
os.output(math.tan(45 * 3.14 / 180))

os.output("")
os.output("--- math.asin/acos/atan ---")
os.output(math.asin(0.5))
os.output(math.acos(0.5))
os.output(math.atan(1))