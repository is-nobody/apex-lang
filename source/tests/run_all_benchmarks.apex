// source/tests/run_all_benchmarks.apex
// Implementation of Benchmark Runner for Apex Language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// recursion
os.execute("apex source/tests/benchmark/fib_recursive.apex")
os.execute("apex source/tests/benchmark/tree.apex")

// loops & iteration
os.execute("apex source/tests/benchmark/for_counter.apex")
os.execute("apex source/tests/benchmark/for_condition.apex")

// string operations
os.execute("apex source/tests/benchmark/string_building.apex")
os.execute("apex source/tests/benchmark/string_key_operations.apex")

// table operations
os.execute("apex source/tests/benchmark/hashtable_large.apex")
os.execute("apex source/tests/benchmark/table_copy.apex")
os.execute("apex source/tests/benchmark/table_merge.apex")
os.execute("apex source/tests/benchmark/table_nested_access.apex")
os.execute("apex source/tests/benchmark/temp_tables.apex")
os.execute("apex source/tests/benchmark/nested_3d_loops.apex")
os.execute("apex source/tests/benchmark/table_int_index.apex")

// matrix & arithmetic
os.execute("apex source/tests/benchmark/matrix_multiplication.apex")