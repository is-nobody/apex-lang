// source/tests/run_all_tests.apex
// Implementation of Test Runner for Apex Language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// arithmetic tests
os.execute("apex source/tests/arithmetic/whole_with_whole.apex")
os.execute("apex source/tests/arithmetic/decimal_with_decimal.apex")
os.execute("apex source/tests/arithmetic/whole_with_decimal.apex")
os.execute("apex source/tests/arithmetic/priority_without_parentheses.apex")
os.execute("apex source/tests/arithmetic/priority_parentheses.apex")

// conditionals - if-elseif-else branches
os.execute("apex source/tests/conditionals/if-elseif-else/if_single.apex")
os.execute("apex source/tests/conditionals/if-elseif-else/if_ternary.apex")
os.execute("apex source/tests/conditionals/if-elseif-else/if_elseif_else.apex")
os.execute("apex source/tests/conditionals/if-elseif-else/if_else.apex")
os.execute("apex source/tests/conditionals/if-elseif-else/nested_if.apex")

// conditionals - comparison operators
os.execute("apex source/tests/conditionals/comparisons/whole.apex")
os.execute("apex source/tests/conditionals/comparisons/decimal.apex")
os.execute("apex source/tests/conditionals/comparisons/string.apex")
os.execute("apex source/tests/conditionals/comparisons/table.apex")
os.execute("apex source/tests/conditionals/comparisons/none.apex")
os.execute("apex source/tests/conditionals/comparisons/boolean.apex")

// conditionals - logical operators
os.execute("apex source/tests/conditionals/logical/and.apex")
os.execute("apex source/tests/conditionals/logical/not.apex")
os.execute("apex source/tests/conditionals/logical/or.apex")

// loops - all iteration constructs
os.execute("apex source/tests/loops/for_condition.apex")
os.execute("apex source/tests/loops/for_counter.apex")
os.execute("apex source/tests/loops/for_table.apex")

// type conversion
os.execute("apex source/tests/type_conversion/number.apex")
os.execute("apex source/tests/type_conversion/string.apex")

// integration tests
os.execute("apex source/tests/integration/function.apex")
os.execute("apex source/tests/integration/table.apex")
os.execute("apex source/tests/integration/import.apex")
os.execute("apex source/tests/integration/constant.apex")

// standard library tests
os.execute("apex source/tests/libraries/os.apex first second")
os.execute("apex source/tests/libraries/sys.apex")
os.execute("apex source/tests/libraries/math.apex")
os.execute("apex source/tests/libraries/random.apex")
os.execute("apex source/tests/libraries/ffi.apex")
os.execute("apex source/tests/libraries/json.apex")
os.execute("apex source/tests/libraries/xml.apex")
os.execute("apex source/tests/libraries/csv.apex")
os.execute("apex source/tests/libraries/base.apex")
os.execute("apex source/tests/libraries/table.apex")
os.execute("apex source/tests/libraries/string.apex")
os.execute("apex source/tests/libraries/regex.apex")
os.execute("apex source/tests/libraries/crypto.apex")