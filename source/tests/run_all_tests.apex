import os
os.execute("
# ARITHMETIC
apex source/tests/arithmetic/whole_with_whole.apex
apex source/tests/arithmetic/decimal_with_decimal.apex
apex source/tests/arithmetic/whole_with_decimal.apex
apex source/tests/arithmetic/priority_without_parentheses.apex
apex source/tests/arithmetic/priority_parentheses.apex

# CONDITIONALS
## IF-ELIF-ELSE
apex source/tests/conditionals/if-elif-else/if_single.apex
apex source/tests/conditionals/if-elif-else/if_elif_else.apex
apex source/tests/conditionals/if-elif-else/if_else.apex
apex source/tests/conditionals/if-elif-else/nested_if.apex
## COMPARISONS
apex source/tests/conditionals/comparisons/whole.apex
apex source/tests/conditionals/comparisons/decimal.apex
apex source/tests/conditionals/comparisons/string.apex
apex source/tests/conditionals/comparisons/boolean.apex
## LOGICAL
apex source/tests/conditionals/logical/and.apex
apex source/tests/conditionals/logical/not.apex
apex source/tests/conditionals/logical/or.apex

# LOOPS
apex source/tests/loops/for_condition.apex
apex source/tests/loops/for_counter.apex
apex source/tests/loops/for_table.apex

# TYPE CONVERSION
apex source/tests/type_conversion/number.apex
apex source/tests/type_conversion/string.apex

# INTEGRATION
apex source/tests/integration/function.apex
apex source/tests/integration/table.apex
apex source/tests/integration/import.apex

# LIBRARIES
apex source/tests/libraries/os.apex
apex source/tests/libraries/sys.apex
apex source/tests/libraries/math.apex
apex source/tests/libraries/random.apex
apex source/tests/libraries/ffi.apex
apex source/tests/libraries/codecs.apex
apex source/tests/libraries/table.apex
apex source/tests/libraries/string.apex
")