// source/tests/errors/for.apex
// Implementation of For Loop Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: for requires a comparison, got number
var = 5

for var     // For requires a comparison, got number
    i = 0

// error: expected end value after ','
for i = 1,  // Expected end value after ','
    i = 0

// error: expected step value after ','
for i = 1, 2,  // Expected step value after ','
    i = 0

// error: expected indented block after 'for'
for i = 1, 2, 3,  // Expected indented block after 'for'
    i = 0

// error: for loop step cannot be zero
for i = 1, 2, 0   // For loop step cannot be zero
    i = 0

// error: unreachable code after control flow statement
for i = 1, 10
    if i == 5
        continue
        i = 5  // Unreachable code after control flow statement

// error: for loop table must be a table, got string
nontable = "string"

for item = nontable  // For loop table must be a table, got string
    item = 0

// errors: break and continue outside of loop
break     // 'break' outside of loop
continue  // 'continue' outside of loop