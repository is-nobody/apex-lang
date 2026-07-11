import random
import os
import table
import string

tests_passed = 0
tests_failed = 0

function assert_equal(actual, expected, name)
    if actual == expected
        tests_passed = tests_passed + 1
    else
        tests_failed = tests_failed + 1
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

function betavariate_range()
    beta = random.betavariate(2, 5)
    is_valid = type(beta) == "number" and beta >= 0 and beta <= 1
    assert_equal(is_valid, true, "betavariate range [0,1]")

function betavariate_negative_alpha()
    res = random.betavariate(-1, 2)
    assert_equal(res, none, "betavariate negative alpha")

function betavariate_negative_beta()
    res = random.betavariate(2, -1)
    assert_equal(res, none, "betavariate negative beta")

function choice_valid()
    t = [10, 20, 30, 40, 50]
    c = random.choice(t)
    found = false
    if c == 10 or c == 20 or c == 30 or c == 40 or c == 50
        found = true
    assert_equal(found, true, "choice returns element from table")

function choice_empty()
    empty_t = []
    res = random.choice(empty_t)
    assert_equal(res, none, "choice empty table")

function compare_digest()
    a = "secret_token_123"
    b = "secret_token_123"
    res = random.compare_digest(a, b)
    assert_equal(res, true, "compare_digest identical strings")

function expovariate_positive()
    exp = random.expovariate(1.0)
    is_valid = type(exp) == "number" and exp >= 0
    assert_equal(is_valid, true, "expovariate positive")

function expovariate_zero_lambda()
    res = random.expovariate(0.0)
    assert_equal(res, none, "expovariate zero lambda")

function gauss_returns_number()
    g = random.gauss(0, 1)
    assert_equal(type(g) == "number", true, "gauss returns number")

function randint_valid()
    random.seed(100)
    i1 = random.randint(1, 10)
    is_valid = type(i1) == "number" and i1 >= 1 and i1 <= 10
    assert_equal(is_valid, true, "randint(1, 10) valid")

function randint_same_bounds()
    i2 = random.randint(5, 5)
    assert_equal(i2, 5, "randint(5, 5)")

function randint_reverse_bounds()
    i3 = random.randint(10, 1)
    is_valid = type(i3) == "number" and i3 >= 1 and i3 <= 10
    assert_equal(is_valid, true, "randint(10, 1)")

function random_range()
    val = random.random()
    is_valid = type(val) == "number" and val >= 0.0 and val < 1.0
    assert_equal(is_valid, true, "random range [0,1)")

function sample_valid()
    t_samp = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    s = random.sample(t_samp, 3)
    is_valid = type(s) == "table" and table.size(s) == 3
    assert_equal(is_valid, true, "sample size correct")

function sample_k_zero()
    t_samp = [1, 2, 3]
    s = random.sample(t_samp, 0)
    is_valid = type(s) == "table" and table.size(s) == 0
    assert_equal(is_valid, true, "sample k=0 empty table")

function sample_k_too_large()
    t_samp = [1, 2, 3]
    res = random.sample(t_samp, 20)
    assert_equal(res, none, "sample k > size")

function secure_randint()
    n = 100
    res = random.secure_randint(n)
    is_valid = res >= 0 and res < n
    assert_equal(is_valid, true, "secure_randint range [0,99]")

function seed_reproducibility()
    random.seed(12345)
    r1 = random.random()
    random.seed(12345)
    r2 = random.random()
    assert_equal(r1, r2, "seed reproducibility")

function seed_no_args()
    random.seed()
    r = random.random()
    assert_equal(type(r) == "number", true, "seed no args")

function shuffle_preserves_size()
    t_shuf = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    original_size = table.size(t_shuf)
    random.shuffle(t_shuf)
    assert_equal(table.size(t_shuf), original_size, "shuffle preserves size")

function shuffle_single_element()
    t_single = ["val" = 1]
    random.shuffle(t_single)
    assert_equal(table.size(t_single), 1, "shuffle single element")

function shuffle_empty()
    t_empty = []
    res = random.shuffle(t_empty)
    assert_equal(res, none, "shuffle empty")

function secure_token_bytes()
    n = 16
    bytes_str = random.secure_token_bytes(n)
    assert_equal(string.len(bytes_str), n, "secure_token_bytes length")

function secure_token_hex()
    nbytes = 16
    hex_str = random.secure_token_hex(nbytes)
    assert_equal(string.len(hex_str), nbytes * 2, "secure_token_hex length")

function triangular_range()
    tri = random.triangular(0, 10, 5)
    is_valid = type(tri) == "number" and tri >= 0 and tri <= 10
    assert_equal(is_valid, true, "triangular range")

function triangular_defaults()
    tri = random.triangular()
    is_valid = type(tri) == "number" and tri >= 0 and tri <= 1
    assert_equal(is_valid, true, "triangular defaults")

betavariate_range()
betavariate_negative_alpha()
betavariate_negative_beta()
choice_valid()
choice_empty()
compare_digest()
expovariate_positive()
expovariate_zero_lambda()
gauss_returns_number()
randint_valid()
randint_same_bounds()
randint_reverse_bounds()
random_range()
sample_valid()
sample_k_zero()
sample_k_too_large()
secure_randint()
seed_reproducibility()
seed_no_args()
shuffle_preserves_size()
shuffle_single_element()
shuffle_empty()
secure_token_bytes()
secure_token_hex()
triangular_range()
triangular_defaults()

if tests_failed == 0
    os.output("[OK] libraries/random.apex")