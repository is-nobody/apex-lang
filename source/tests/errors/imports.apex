// source/tests/errors/imports.apex
// Implementation of Import Error Test Cases for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

// error: module 'notexists' not found
import notexists  // Module 'notexists' not found

// error: expected module name after '.'
import expected.  // Expected module name after '.'

// error: expected module name
import  // Expected module name