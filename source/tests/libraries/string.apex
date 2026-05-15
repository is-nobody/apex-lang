import os
import string

os.output("=== STRING TESTS ===")
os.output("")

// 1. len
os.output("1. len:")
l = string.len("Hello")
os.output("   Hello length: {l}")
os.output("")

// 2. lower
os.output("2. lower:")
low = string.lower("HELLO")
os.output("   {low}")
os.output("")

// 3. upper
os.output("3. upper:")
up = string.upper("hello")
os.output("   {up}")
os.output("")

// 4. sub
os.output("4. sub:")
s = string.sub("Hello World", 0, 5)
os.output("   {s}")
os.output("")

// 5. split
os.output("5. split:")
parts = string.split("a,b,c", ",")
os.output("   {parts}")
os.output("")

// 6. join (using table from split)
os.output("6. join:")
j = string.join(parts, "-")
os.output("   {j}")
os.output("")

// 7. trim
os.output("7. trim:")
t = string.trim("  hello  ")
os.output("   {t}")
os.output("")

// 8. find
os.output("8. find:")
f = string.find("Hello World", "World")
os.output("   'World' found at: {f}")
os.output("")

// 9. replace
os.output("9. replace:")
rep = string.replace("Hello World", "World", "Apex")
os.output("   {rep}")
os.output("")

os.output("=== DONE ===")