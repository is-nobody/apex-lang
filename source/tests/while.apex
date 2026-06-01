import os


// Test 1: Simple while loop with counter
counter = 1
output_string = ""
expected_string = "12345"

while counter <= 5
    output_string = "{output_string}{counter}"
    counter = counter + 1

os.output("Simple while loop, expected {expected_string}, got {output_string}")

// Test 2: While loop that starts false (should not execute)
counter = 10
executed = false
while counter < 5
    executed = true
    counter = counter + 1

os.output("While loop with false condition executed when it shouldn't = {result}")

// Test 3: While loop with decrement
counter = 5
output_string = ""
expected_string = "54321"

while counter >= 1
    output_string = "{output_string}{counter}"
    counter = counter - 1

os.output("Decrementing while loop, expected {expected_string}, got {output_string}")

// Test 4: While loop with break
x = 1
output_string = ""
expected_string = "1234"

while x <= 10
    if x == 5
        break
    output_string = "{output_string}{x}"
    x = x + 1

os.output("While loop with break, expected {expected_string}, got {output_string}")

// Test 5: While loop with continue
x = 0
output_string = ""
expected_string = "13579"

while x < 10
    x = x + 1
    if x % 2 == 0
        continue
    output_string = "{output_string}{x}"

os.output("While loop with continue, expected {expected_string}, got {output_string}")

// Test 6: While loop with arithmetic in condition (0+1+2+3+4 = 10)
counter = 0
total = 0
while counter * counter < 20
    total = total + counter
    counter = counter + 1

os.output("While with arithmetic condition, expected total 10, got {total}")

// Test 7: Nested while loops (expected 1*1+1*2+2*1+2*2+3*1+3*2 = 1+2+2+4+3+6 = 18)
outer = 1
sum_result = 0

while outer <= 3
    inner = 1
    while inner <= 2
        sum_result = sum_result + outer * inner
        inner = inner + 1
    outer = outer + 1

os.output("Nested while loops, expected sum 18, got {sum_result}")

// Test 8: While loop that accumulates product (5! = 120)
counter = 1
product = 1
while counter <= 5
    product = product * counter
    counter = counter + 1

os.output("Factorial of 5, expected 120, got {product}")

// Test 9: While loop counting evens (2,4,6,8,10 = 5)
counter = 1
even_count = 0
while counter <= 10
    if counter % 2 == 0
        even_count = even_count + 1
    counter = counter + 1

os.output("Count evens from 1-10, expected 5, got {even_count}")

// Test 10: While loop with floating point (3.0→1.5→0.75→0.375→0.1875→0.09375 = 5 iterations)
x = 3.0
iterations = 0
while x > 0.1
    x = x / 2
    iterations = iterations + 1

os.output("Float division while loop, expected iterations=5, got {iterations}")

// Test 11: While loop - count numbers divisible by 6 (6,12,18,24,30,36,42,48 = 8)
n = 50
count_divisible = 0
while n >= 1
    if n % 3 == 0 and n % 2 == 0
        count_divisible = count_divisible + 1
    n = n - 1

os.output("Count divisible by 6 from 1-50, expected 8, got {count_divisible}")

// final result
