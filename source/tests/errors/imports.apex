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

// error: cannot use 'as' alias with built-in module
import os as myos  // Cannot use 'as' alias with built-in module 'os'

// error: expected alias name after 'as'
import lib.apex as  // Expected alias name after 'as'

// error: expected alias name after 'as'
import lib.apex as 123  // Expected alias name after 'as'

// error: expected alias name after 'as'
import lib.apex as "alias"  // Expected alias name after 'as'

// error: unexpected token after import
import lib.apex as mylib extra  // Unexpected token after import

// error: unexpected token after import
import lib.apex as mylib.extra  // Unexpected token after import

// error: module 'mylib' not found
import lib2.apex as mylib  // Module 'mylib' not found

// error: expected alias name after 'as'
import lib.apex as .  // Expected alias name after 'as'

// error: expected alias name after 'as'
import lib.apex as /  // Expected alias name after 'as'