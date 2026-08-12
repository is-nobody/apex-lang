// source/tests/errors/return.apex
// Implementation of Return Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: expected return value
function empty_return()
    return  // Expected return value

// error: unexpected token after return
function return_with_operator()
    return +  // Unexpected token after return

// error: unexpected token after return
function return_with_comparison()
    return ==  // Unexpected token after return

// error: unexpected token after return
function return_with_and()
    return and  // Unexpected token after return

// error: unexpected token after return
function return_with_or()
    return or  // Unexpected token after return

// error: unexpected token after return
function return_with_dot()
    return none.  // Unexpected token after return

// error: unexpected token after return
function return_with_comma()
    return none,  // Unexpected token after return

// error: unexpected token after return
function return_with_double_comma()
    return none,,  // Unexpected token after return

// error: multiple return values are restricted
function return_multiple()
    return none, true  // Multiple return values are restricted

// error: multiple return values are restricted
function return_triple()
    return none, true, false  // Multiple return values are restricted

// error: unexpected token after return
function return_value_comma_space()
    return none,   // Unexpected token after return

// error: unexpected token after return
function return_nothing_after_comma()
    return none, // Unexpected token after return

// error: unexpected token after return
function return_with_bracket()
    return )  // Unexpected token after return

// error: unexpected token after return
function return_with_rbracket()
    return ]  // Unexpected token after return

// error: unexpected token after return
function return_with_else()
    return else  // Unexpected token after return

// error: unexpected token after return
function return_with_elif()
    return elif  // Unexpected token after return

// error: unexpected token after return
function return_with_dot_number()
    return .5  // Unexpected token after return