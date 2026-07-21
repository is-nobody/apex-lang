import os

// ARITHMETIC
os.execute("apex source/tests/arithmetic/whole_with_whole.apex")
os.execute("apex source/tests/arithmetic/decimal_with_decimal.apex")
os.execute("apex source/tests/arithmetic/whole_with_decimal.apex")
os.execute("apex source/tests/arithmetic/priority_without_parentheses.apex")
os.execute("apex source/tests/arithmetic/priority_parentheses.apex")

// CONDITIONALS
// IF-ELIF-ELSE
os.execute("apex source/tests/conditionals/if-elif-else/if_single.apex")
os.execute("apex source/tests/conditionals/if-elif-else/if_ternary.apex")
os.execute("apex source/tests/conditionals/if-elif-else/if_elif_else.apex")
os.execute("apex source/tests/conditionals/if-elif-else/if_else.apex")
os.execute("apex source/tests/conditionals/if-elif-else/nested_if.apex")

// COMPARISONS
os.execute("apex source/tests/conditionals/comparisons/whole.apex")
os.execute("apex source/tests/conditionals/comparisons/decimal.apex")
os.execute("apex source/tests/conditionals/comparisons/string.apex")
os.execute("apex source/tests/conditionals/comparisons/table.apex")
os.execute("apex source/tests/conditionals/comparisons/none.apex")
os.execute("apex source/tests/conditionals/comparisons/boolean.apex")

// LOGICAL
os.execute("apex source/tests/conditionals/logical/and.apex")
os.execute("apex source/tests/conditionals/logical/not.apex")
os.execute("apex source/tests/conditionals/logical/or.apex")

// LOOPS
os.execute("apex source/tests/loops/for_condition.apex")
os.execute("apex source/tests/loops/for_counter.apex")
os.execute("apex source/tests/loops/for_table.apex")

// TYPE CONVERSION
os.execute("apex source/tests/type_conversion/number.apex")
os.execute("apex source/tests/type_conversion/string.apex")

// INTEGRATION
os.execute("apex source/tests/integration/function.apex")
os.execute("apex source/tests/integration/table.apex")
os.execute("apex source/tests/integration/import.apex")

// LIBRARIES
os.execute("apex source/tests/libraries/os.apex")
os.execute("apex source/tests/libraries/sys.apex")
os.execute("apex source/tests/libraries/math.apex")
os.execute("apex source/tests/libraries/random.apex")
os.execute("apex source/tests/libraries/ffi.apex")
os.execute("apex source/tests/libraries/codecs.apex")
os.execute("apex source/tests/libraries/table.apex")
os.execute("apex source/tests/libraries/string.apex")