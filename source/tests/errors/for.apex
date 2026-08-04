var = 5

for var     // For requires a comparison, got number
    i = 0

for i = 1,  // Expected end value after ','
    i = 0

for i = 1, 2,  // Expected step value after ','
    i = 0

for i = 1, 2, 3,  // Expected indented block after 'for'
    i = 0

for i = 1, 2, 0   // For loop step cannot be zero
    i = 0

for i = 1, 10
    if i == 5
        continue
        i = 5  // Unreachable code after control flow statement

break     // 'break' outside of loop
continue  // 'continue' outside of loop