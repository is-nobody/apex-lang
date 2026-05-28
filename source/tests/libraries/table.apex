import os
import table

os.output("=== TABLE TESTS ===")
os.output("")

// 1. table.size
os.output("1. table.size:")
t = (1, 2, 3)
s = table.size(t)
os.output(s)
os.output("")

// 2. table.has
os.output("2. table.has:")
t = (name = "Alice", age = 30)
h1 = table.has(t, "name")
h2 = table.has(t, "city")
os.output(h1)
os.output(h2)
os.output("")

// 3. table.keys
os.output("3. table.keys:")
t = (name = "Alice", age = 30)
k = table.keys(t)
os.output(k)
os.output("")

// 4. table.values
os.output("4. table.values:")
t = (name = "Alice", age = 30)
v = table.values(t)
os.output(v)
os.output("")

// 5. table.remove
os.output("5. table.remove:")
t = ("a", "b", "c")
os.output("Before:")
os.output(t)
table.remove(t, 2)
os.output("After:")
os.output(t)
os.output("")

// 6. table.clear
os.output("6. table.clear:")
t = (1, 2, 3)
os.output("Before:")
os.output(t)
table.clear(t)
os.output("After:")
os.output(t)
os.output("")

// 7. table.copy
os.output("7. table.copy:")
original = (x = 1, y = 2)
copy = table.copy(original)
copy.x = 999
os.output("Original:")
os.output(original)
os.output("Copy:")
os.output(copy)
os.output("")

// 8. table.merge
os.output("8. table.merge:")
a = (1, 2)
b = (3, 4)
m = table.merge(a, b)
os.output(m)
os.output("")

os.output("=== DONE ===")