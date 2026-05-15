import os

// whole with whole
os.output(10 + 3)
os.output(10 - 3)
os.output(10 * 3)
os.output(10 / 3)
os.output(10 % 3)

// decimal with decimal
os.output(3.5 + 1.5)
os.output(3.5 - 1.5)
os.output(3.5 * 1.5)
os.output(3.5 / 1.5)
os.output(3.5 % 1.5)

// whole with decimal
os.output(5 + 2.5)
os.output(5 - 2.5)
os.output(5 * 2.5)
os.output(5 / 2.5)
os.output(5 % 2.5)

// priority - same precedence left to right
os.output(20 - 5 - 3)
os.output(40 / 4 / 2)
os.output(10 - 5 + 2)
os.output(12 * 3 / 2)

// priority - mixed operators without parentheses
os.output(4 * 3 + 5)
os.output(4 + 3 * 5)
os.output(30 / 3 + 4)
os.output(30 + 9 / 3)
os.output(18 - 2 * 4)
os.output(2 * 8 - 3)
os.output(24 / 4 - 3)
os.output(24 - 12 / 3)
os.output(13 % 5 + 2)
os.output(13 + 5 % 3)

// priority - parentheses changing order
os.output((2 + 3) * 5)
os.output(2 * (5 - 3))
os.output(20 / (3 + 2))
os.output((8 - 3) * (2 + 1))

// priority - multiple levels
os.output(2 + 3 * 4 - 1)
os.output(5 * 2 + 3 * 5)
os.output(20 / 4 - 3 * 1 + 2)
os.output(10 % 3 + 2 * 4)