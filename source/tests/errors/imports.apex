// source/tests/errors/imports.apex
// Implementation of Import Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: import path must end with '.apex'
import file  // Import path must end with '.apex'

// error: import path must end with '.apex'
import file.  // Import path must end with '.apex'

// error: expected module name after '/'
import expected/  // Expected module name after '/'

// error: module 'notexists' not found
import notexists.apex  // Module 'notexists' not found

// error: unexpected token after import
import .ts  // Unexpected token after import

// error: unexpected token after import
import >ts.apex  // Unexpected token after import

// error: expected module name
import  // Expected module name