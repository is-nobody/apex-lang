import os

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function table_equal_empty()
    t1 = []
    t2 = []
    assert_equal(t1 == t2, true, "empty tables equal")
    assert_equal(t1 != t2, false, "empty tables not equal")

function table_equal_same_keys()
    t1 = ["a" = 1, "b" = 2]
    t2 = ["a" = 1, "b" = 2]
    assert_equal(t1 == t2, true, "same keys and values")
    assert_equal(t1 != t2, false, "same keys and values not equal")

function table_equal_different_value()
    t1 = ["a" = 1, "b" = 2]
    t2 = ["a" = 1, "b" = 3]
    assert_equal(t1 == t2, false, "different values")
    assert_equal(t1 != t2, true, "different values not equal")

function table_equal_different_keys()
    t1 = ["a" = 1, "b" = 2]
    t2 = ["a" = 1, "c" = 2]
    assert_equal(t1 == t2, false, "different keys")
    assert_equal(t1 != t2, true, "different keys not equal")

function table_equal_different_size()
    t1 = ["a" = 1]
    t2 = ["a" = 1, "b" = 2]
    assert_equal(t1 == t2, false, "different size")
    assert_equal(t1 != t2, true, "different size not equal")

function table_equal_nested()
    t1 = ["a" = 1, "b" = ["x" = 10, "y" = 20]]
    t2 = ["a" = 1, "b" = ["x" = 10, "y" = 20]]
    assert_equal(t1 == t2, true, "nested tables equal")
    assert_equal(t1 != t2, false, "nested tables not equal")

function table_equal_nested_different()
    t1 = ["a" = 1, "b" = ["x" = 10, "y" = 20]]
    t2 = ["a" = 1, "b" = ["x" = 10, "y" = 30]]
    assert_equal(t1 == t2, false, "nested tables different")
    assert_equal(t1 != t2, true, "nested tables different not equal")

function table_equal_deep_nested()
    t1 = ["a" = ["b" = ["c" = ["d" = 42]]]]
    t2 = ["a" = ["b" = ["c" = ["d" = 42]]]]
    assert_equal(t1 == t2, true, "deep nested equal")
    assert_equal(t1 != t2, false, "deep nested not equal")

function table_equal_deep_nested_different()
    t1 = ["a" = ["b" = ["c" = ["d" = 42]]]]
    t2 = ["a" = ["b" = ["c" = ["d" = 43]]]]
    assert_equal(t1 == t2, false, "deep nested different")
    assert_equal(t1 != t2, true, "deep nested different not equal")

function table_equal_with_array()
    t1 = ["a" = [1, 2, 3], "b" = 4]
    t2 = ["a" = [1, 2, 3], "b" = 4]
    assert_equal(t1 == t2, true, "tables with arrays equal")
    assert_equal(t1 != t2, false, "tables with arrays not equal")

function table_equal_with_array_different()
    t1 = ["a" = [1, 2, 3], "b" = 4]
    t2 = ["a" = [1, 2, 4], "b" = 4]
    assert_equal(t1 == t2, false, "tables with arrays different")
    assert_equal(t1 != t2, true, "tables with arrays different not equal")

function table_equal_mixed()
    t1 = ["a" = 1, "b" = "hello", "c" = true, "d" = none]
    t2 = ["a" = 1, "b" = "hello", "c" = true, "d" = none]
    assert_equal(t1 == t2, true, "mixed types equal")
    assert_equal(t1 != t2, false, "mixed types not equal")

function table_equal_mixed_different()
    t1 = ["a" = 1, "b" = "hello", "c" = true, "d" = none]
    t2 = ["a" = 1, "b" = "world", "c" = true, "d" = none]
    assert_equal(t1 == t2, false, "mixed types different")
    assert_equal(t1 != t2, true, "mixed types different not equal")

function table_equal_self()
    t1 = ["a" = 1, "b" = 2]
    assert_equal(t1 == t1, true, "table equal to itself")
    assert_equal(t1 != t1, false, "table not equal to itself")

function table_equal_with_none()
    t1 = ["a" = none, "b" = 2]
    t2 = ["a" = none, "b" = 2]
    assert_equal(t1 == t2, true, "tables with none equal")
    assert_equal(t1 != t2, false, "tables with none not equal")

function table_equal_none_vs_missing()
    t1 = ["a" = none, "b" = 2]
    t2 = ["b" = 2]
    assert_equal(t1 == t2, false, "none vs missing key")
    assert_equal(t1 != t2, true, "none vs missing key not equal")

function table_equal_order_independent()
    t1 = ["a" = 1, "b" = 2, "c" = 3]
    t2 = ["c" = 3, "a" = 1, "b" = 2]
    assert_equal(t1 == t2, true, "order independent equal")
    assert_equal(t1 != t2, false, "order independent not equal")

function table_equal_with_string_keys()
    t1 = ["key1" = "value1", "key2" = "value2"]
    t2 = ["key1" = "value1", "key2" = "value2"]
    assert_equal(t1 == t2, true, "string keys equal")
    assert_equal(t1 != t2, false, "string keys not equal")

function table_equal_with_numeric_keys()
    t1 = [1, 2, 3]
    t2 = [1, 2, 3]
    assert_equal(t1 == t2, true, "numeric keys equal")
    assert_equal(t1 != t2, false, "numeric keys not equal")

function table_equal_with_numeric_keys_different()
    t1 = [1, 2, 3]
    t2 = [1, 2, 4]
    assert_equal(t1 == t2, false, "numeric keys different")
    assert_equal(t1 != t2, true, "numeric keys different not equal")

function table_equal_function_compare()
    function add(a, b)
        return a + b
    
    t1 = ["func" = add]
    t2 = ["func" = add]
    assert_equal(t1 == t2, true, "tables with same function equal")
    assert_equal(t1 != t2, false, "tables with same function not equal")

function table_equal_function_different()
    function add(a, b)
        return a + b
    
    function sub(a, b)
        return a - b
    
    t1 = ["func" = add]
    t2 = ["func" = sub]
    assert_equal(t1 == t2, false, "tables with different functions not equal")
    assert_equal(t1 != t2, true, "tables with different functions not equal")

function table_equal_recursive_limit()
    t1 = []
    t1["self"] = t1
    
    t2 = []
    t2["self"] = t2
    
    assert_equal(t1 == t2, true, "recursive tables equal (depth limit 100)")
    assert_equal(t1 != t2, false, "recursive tables not equal")

function table_equal_with_float_keys()
    t1 = ["1.5" = "one", "2.5" = "two"]
    t2 = ["1.5" = "one", "2.5" = "two"]
    assert_equal(t1 == t2, true, "float keys equal")
    assert_equal(t1 != t2, false, "float keys not equal")

function table_equal_sparse()
    t1 = []
    t1[1] = "one"
    t1[1000] = "thousand"
    
    t2 = []
    t2[1] = "one"
    t2[1000] = "thousand"
    
    assert_equal(t1 == t2, true, "sparse tables equal")
    assert_equal(t1 != t2, false, "sparse tables not equal")

function table_equal_sparse_different()
    t1 = []
    t1[1] = "one"
    t1[1000] = "thousand"
    
    t2 = []
    t2[1] = "one"
    t2[1000] = "million"
    
    assert_equal(t1 == t2, false, "sparse tables different")
    assert_equal(t1 != t2, true, "sparse tables different not equal")

function table_equal_mixed_string_numeric()
    t1 = ["a" = 1, 2 = "b"]
    t2 = ["a" = 1, 2 = "b"]
    assert_equal(t1 == t2, true, "mixed string/numeric keys equal")
    assert_equal(t1 != t2, false, "mixed string/numeric keys not equal")

function table_equal_large()
    t1 = []
    t2 = []
    for i = 1, 1000
        t1[i] = i
        t2[i] = i
    assert_equal(t1 == t2, true, "large tables equal")
    assert_equal(t1 != t2, false, "large tables not equal")

function table_equal_large_different()
    t1 = []
    t2 = []
    for i = 1, 1000
        t1[i] = i
        t2[i] = i
    t2[500] = 999
    assert_equal(t1 == t2, false, "large tables different")
    assert_equal(t1 != t2, true, "large tables different not equal")

function table_equal_with_boolean_keys()
    t1 = ["true" = "yes", "false" = "no"]
    t2 = ["true" = "yes", "false" = "no"]
    assert_equal(t1 == t2, true, "boolean keys equal")
    assert_equal(t1 != t2, false, "boolean keys not equal")

function table_equal_empty_array()
    t1 = []
    t2 = []
    assert_equal(t1 == t2, true, "empty array tables equal")
    assert_equal(t1 != t2, false, "empty array tables not equal")

function table_equal_with_nested_none()
    t1 = ["a" = ["b" = none]]
    t2 = ["a" = ["b" = none]]
    assert_equal(t1 == t2, true, "nested none equal")
    assert_equal(t1 != t2, false, "nested none not equal")

function table_equal_with_nested_none_different()
    t1 = ["a" = ["b" = none]]
    t2 = ["a" = ["b" = 42]]
    assert_equal(t1 == t2, false, "nested none vs value different")
    assert_equal(t1 != t2, true, "nested none vs value not equal")

function table_equal_after_modification()
    t1 = ["a" = 1, "b" = 2]
    t2 = ["a" = 1, "b" = 2]
    
    assert_equal(t1 == t2, true, "before modification equal")
    
    t2["b"] = 3
    assert_equal(t1 == t2, false, "after modification different")
    assert_equal(t1 != t2, true, "after modification not equal")

function table_equal_negative_zero()
    t1 = []
    t1[-0.0] = "value"
    t2 = []
    t2[0.0] = "value"
    assert_equal(t1 == t2, true, "-0.0 and 0.0 keys treated equal")
    assert_equal(t1 != t2, false, "-0.0 and 0.0 keys not equal")

function table_equal_nan_keys()
    nan = 0.0 / 0.0
    t1 = []
    t1[nan] = "nan value"
    t2 = []
    t2[nan] = "nan value"
    assert_equal(t1 == t2, false, "NaN keys never equal (NaN != NaN)")
    assert_equal(t1 != t2, true, "NaN keys not equal")

function table_equal_infinity_keys()
    t1 = [pos_inf = 1.0 / 0.0, neg_inf = -1.0 / 0.0]
    t2 = [pos_inf = 1.0 / 0.0, neg_inf = -1.0 / 0.0]
    assert_equal(t1 == t2, true, "infinity keys equal")
    assert_equal(t1 != t2, false, "infinity keys not equal")

table_equal_empty()
table_equal_same_keys()
table_equal_different_value()
table_equal_different_keys()
table_equal_different_size()
table_equal_nested()
table_equal_nested_different()
table_equal_deep_nested()
table_equal_deep_nested_different()
table_equal_with_array()
table_equal_with_array_different()
table_equal_mixed()
table_equal_mixed_different()
table_equal_self()
table_equal_with_none()
table_equal_none_vs_missing()
table_equal_order_independent()
table_equal_with_string_keys()
table_equal_with_numeric_keys()
table_equal_with_numeric_keys_different()
table_equal_function_compare()
table_equal_function_different()
table_equal_recursive_limit()
table_equal_with_float_keys()
table_equal_sparse()
table_equal_sparse_different()
table_equal_mixed_string_numeric()
table_equal_large()
table_equal_large_different()
table_equal_with_boolean_keys()
table_equal_empty_array()
table_equal_with_nested_none()
table_equal_with_nested_none_different()
table_equal_after_modification()
table_equal_negative_zero()
table_equal_nan_keys()
table_equal_infinity_keys()

if tests_failed == 0
    os.output("[OK] conditionals/comparisons/table.apex")