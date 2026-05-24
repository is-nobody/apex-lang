import os

errors = 0

// Test 1: Simple while loop with counter
counter = 1
output_string = ""
expected_string = "12345"

while counter <= 5
    output_string = "{output_string}{counter}"
    counter = counter + 1

if output_string != expected_string
    os.output("  ERROR: Simple while loop expected {expected_string}, got {output_string}")
    errors = errors + 1

// Test 2: While loop that starts false (should not execute)
counter = 10
executed = false
while counter < 5
    executed = true
    counter = counter + 1

if executed != false
    os.output("  ERROR: While loop with false condition executed when it shouldn't")
    errors = errors + 1

// Test 3: While loop with decrement
counter = 5
output_string = ""
expected_string = "54321"

while counter >= 1
    output_string = "{output_string}{counter}"
    counter = counter - 1

if output_string != expected_string
    os.output("  ERROR: Decrementing while loop expected {expected_string}, got {output_string}")
    errors = errors + 1

// Test 4: While loop with break
x = 1
output_string = ""
expected_string = "1234"

while x <= 10
    if x == 5
        break
    output_string = "{output_string}{x}"
    x = x + 1

if output_string != expected_string
    os.output("  ERROR: While loop with break expected {expected_string}, got {output_string}")
    errors = errors + 1

// Test 5: While loop with continue
x = 0
output_string = ""
expected_string = "13579"

while x < 10
    x = x + 1
    if x % 2 == 0
        continue
    output_string = "{output_string}{x}"

if output_string != expected_string
    os.output("  ERROR: While loop with continue expected {expected_string}, got {output_string}")
    errors = errors + 1

// Test 6: While loop with arithmetic in condition (0+1+2+3+4 = 10)
counter = 0
total = 0
while counter * counter < 20
    total = total + counter
    counter = counter + 1

if total != 10
    os.output("  ERROR: While with arithmetic condition expected total 10, got {total}")
    errors = errors + 1

// Test 7: Nested while loops (expected 1*1+1*2+2*1+2*2+3*1+3*2 = 1+2+2+4+3+6 = 18)
outer = 1
sum_result = 0

while outer <= 3
    inner = 1
    while inner <= 2
        sum_result = sum_result + outer * inner
        inner = inner + 1
    outer = outer + 1

if sum_result != 18
    os.output("  ERROR: Nested while loops expected sum 18, got {sum_result}")
    errors = errors + 1

// Test 8: While loop that accumulates product (5! = 120)
counter = 1
product = 1
while counter <= 5
    product = product * counter
    counter = counter + 1

if product != 120
    os.output("  ERROR: Factorial of 5 expected 120, got {product}")
    errors = errors + 1

// Test 9: While loop counting evens (2,4,6,8,10 = 5)
counter = 1
even_count = 0
while counter <= 10
    if counter % 2 == 0
        even_count = even_count + 1
    counter = counter + 1

if even_count != 5
    os.output("  ERROR: Count evens from 1-10 expected 5, got {even_count}")
    errors = errors + 1

// Test 10: While loop with floating point (3.0→1.5→0.75→0.375→0.1875→0.09375 = 5 iterations)
x = 3.0
iterations = 0
while x > 0.1
    x = x / 2
    iterations = iterations + 1

if iterations != 5
    os.output("  ERROR: Float division while loop expected iterations=5, got {iterations}")
    errors = errors + 1

// Test 11: While loop - count numbers divisible by 6 (6,12,18,24,30,36,42,48 = 8)
n = 50
count_divisible = 0
while n >= 1
    if n % 3 == 0 and n % 2 == 0
        count_divisible = count_divisible + 1
    n = n - 1

if count_divisible != 8
    os.output("  ERROR: Count divisible by 6 from 1-50 expected 8, got {count_divisible}")
    errors = errors + 1

// final result
os.output("Total errors in while.apex: {errors}")

if errors == 0
    os.output("Tests in while.apex passed!")
else
    os.output("{errors} test(s) failed in while.apex.")