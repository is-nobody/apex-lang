import os

result = ""

// if statement
score = 85
if score >= 90
    result = "A"
elif score >= 80
    result = "B"
elif score >= 70
    result = "C"
else
    result = "F"

os.output("Test 1: score 85 should give B, got {result}")

// if without elif or else
x = 10
if x > 5
    result = "big"
os.output("Test 2: x > 5 should set result to big, got {result}")

// if with else
age = 16
if age >= 18
    result = "adult"
else
    result = "minor"

os.output("Test 3: age 16 should give minor, got {result}")

// multiple elifs
score = 65
if score >= 90
    result = "A"
elif score >= 80
    result = "B"
elif score >= 70
    result = "C"
elif score >= 60
    result = "D"
else
    result = "F"

os.output("Test 4: score 65 should give D, got {result}")

// if with none (none is not false)
value = none
if value == none
    result = "none detected"
else
    result = "has value"

os.output("Test 5: none comparison should work, got {result}")

// if with boolean
is_active = true
if is_active == true
    result = "active"
else
    result = "inactive"

os.output("Test 6: true should give active, got {result}")

// if with comparison result
x = 10
y = 20
if x < y
    result = "less"
else
    result = "greater"

os.output("Test 7: 10 < 20 should give less, got {result}")

// nested if
x = 15
if x > 10
    if x < 20
        result = "between"
    else
        result = "above"
else
    result = "below"

os.output("Test 8: x between 10-20 should give between, got {result}")

// if with == operator
name = "Alice"
if name == "Alice"
    result = "match"
else
    result = "no match"

os.output("Test 9: name Alice should match, got {result}")

// if with != operator
name = "Bob"
if name != "Alice"
    result = "different"
else
    result = "same"

os.output("Test 10: Bob != Alice should give different, got {result}")

// if with and operator
age = 25
has_license = true
if age >= 18 and has_license == true
    result = "can drive"
else
    result = "cannot drive"

os.output("Test 11: age 25 with license should be can drive, got {result}")

// if with or operator
day = "Saturday"
if day == "Saturday" or day == "Sunday"
    result = "weekend"
else
    result = "weekday"

os.output("Test 12: Saturday should be weekend, got {result}")

// if with not operator
is_ready = false
if not is_ready
    result = "not ready"
else
    result = "ready"

os.output("Test 13: not false should give not ready, got {result}")

// if with multiple conditions
temp = 75
if temp < 32
    result = "freezing"
elif temp < 50
    result = "cold"
elif temp < 70
    result = "cool"
elif temp < 85
    result = "warm"
else
    result = "hot"

os.output("Test 14: temp 75 should give warm, got {result}")

// first condition true, rest skipped
result = ""
score = 95
if score >= 90
    result = "A"
elif score >= 80
    result = "B"
elif score >= 70
    result = "C"
else
    result = "F"

os.output("Test 15: first true should stop, got {result}")

// final result
