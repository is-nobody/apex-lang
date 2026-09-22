// source/tests/run_all_tests.apex
// Implementation of Test Runner for Apex Language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

async function run(cmd)
    await os.execute(cmd)

// arithmetic tests
run("apex source/tests/arithmetic/whole_with_whole.apex")
run("apex source/tests/arithmetic/decimal_with_decimal.apex")
run("apex source/tests/arithmetic/whole_with_decimal.apex")
run("apex source/tests/arithmetic/priority_without_parentheses.apex")
run("apex source/tests/arithmetic/priority_parentheses.apex")

// conditionals - comparison operators
run("apex source/tests/conditionals/comparisons/whole.apex")
run("apex source/tests/conditionals/comparisons/decimal.apex")
run("apex source/tests/conditionals/comparisons/string.apex")
run("apex source/tests/conditionals/comparisons/table.apex")
run("apex source/tests/conditionals/comparisons/none.apex")
run("apex source/tests/conditionals/comparisons/boolean.apex")

// conditionals - logical operators
run("apex source/tests/conditionals/logical/and.apex")
run("apex source/tests/conditionals/logical/not.apex")
run("apex source/tests/conditionals/logical/or.apex")

// loops - all iteration constructs
run("apex source/tests/loops/for_condition.apex")
run("apex source/tests/loops/for_counter.apex")
run("apex source/tests/loops/for_table.apex")

// type conversion
run("apex source/tests/type_conversion/number.apex")
run("apex source/tests/type_conversion/string.apex")

// integration tests
run("apex source/tests/integration/if_else.apex")
run("apex source/tests/integration/if_ternary.apex")
run("apex source/tests/integration/function.apex")
run("apex source/tests/integration/table.apex")
run("apex source/tests/integration/import.apex")
run("apex source/tests/integration/constant.apex")
run("apex source/tests/integration/match.apex")
run("apex source/tests/integration/async_await.apex")

// standard library tests
run("apex source/tests/libraries/os.apex first second")
run("apex source/tests/libraries/sys.apex")
run("apex source/tests/libraries/math.apex")
run("apex source/tests/libraries/random.apex")
run("apex source/tests/libraries/json.apex")
run("apex source/tests/libraries/xml.apex")
run("apex source/tests/libraries/csv.apex")
run("apex source/tests/libraries/base.apex")
run("apex source/tests/libraries/table.apex")
run("apex source/tests/libraries/string.apex")
run("apex source/tests/libraries/regex.apex")
run("apex source/tests/libraries/crypto.apex")