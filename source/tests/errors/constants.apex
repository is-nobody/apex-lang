// source/tests/errors/constants.apex
// Implementation of Constants Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: expected variable name after 'constant'
constant  // Expected variable name after 'constant'

// error: expected variable name after 'constant'
constant = 5  // Expected variable name after 'constant'

// error: expected variable name after 'constant'
constant constant y = 1  // Expected variable name after 'constant'

// error: expected variable name after 'constant'
constant 123 = 5  // Expected variable name after 'constant'

// error: constant must be initialized with '='
constant x  // Constant must be initialized with '='

// error: expected expression after '='
constant x =  // Expected expression after '='

// error: cannot reassign constant 'name'
constant name = none
name = true  // Cannot reassign constant 'name'
var = "{name = 5}"  // Cannot reassign constant 'name'