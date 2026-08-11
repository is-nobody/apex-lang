// source/tests/libraries/random.apex
// Implementation of Random Library Function Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import random
import os
import table
import string

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// test: betavariate returns value in [0,1] range
function betavariate_range()
    beta = random.betavariate(2, 5)
    is_valid = type(beta) == "number" and beta >= 0 and beta <= 1
    assert_equal(is_valid, true, "betavariate range [0,1]")

// test: betavariate with negative alpha returns none
function betavariate_negative_alpha()
    res = random.betavariate(-1, 2)
    assert_equal(res, none, "betavariate negative alpha")

// test: betavariate with negative beta returns none
function betavariate_negative_beta()
    res = random.betavariate(2, -1)
    assert_equal(res, none, "betavariate negative beta")

// test: choice returns element from non-empty table
function choice_valid()
    t = [10, 20, 30, 40, 50]
    c = random.choice(t)
    found = false
    if c == 10 or c == 20 or c == 30 or c == 40 or c == 50
        found = true
    assert_equal(found, true, "choice returns element from table")

// test: choice on empty table returns none
function choice_empty()
    empty_t = []
    res = random.choice(empty_t)
    assert_equal(res, none, "choice empty table")

// test: expovariate returns non-negative value
function expovariate_positive()
    exp = random.expovariate(1.0)
    is_valid = type(exp) == "number" and exp >= 0
    assert_equal(is_valid, true, "expovariate positive")

// test: expovariate with zero lambda returns none
function expovariate_zero_lambda()
    res = random.expovariate(0.0)
    assert_equal(res, none, "expovariate zero lambda")

// test: gauss returns a number
function gauss_returns_number()
    g = random.normal(0, 1)
    assert_equal(type(g) == "number", true, "gauss returns number")

// test: randint returns integer within specified range
function randint_valid()
    random.seed(100)
    i1 = random.integer(1, 10)
    is_valid = type(i1) == "number" and i1 >= 1 and i1 <= 10
    assert_equal(is_valid, true, "randint(1, 10) valid")

// test: randint with same bounds returns that value
function randint_same_bounds()
    i2 = random.integer(5, 5)
    assert_equal(i2, 5, "randint(5, 5)")

// test: randint auto-swaps when bounds reversed
function randint_reverse_bounds()
    i3 = random.integer(10, 1)
    is_valid = type(i3) == "number" and i3 >= 1 and i3 <= 10
    assert_equal(is_valid, true, "randint(10, 1)")

// test: random returns number in [0,1) range
function random_range()
    val = random.float()
    is_valid = type(val) == "number" and val >= 0.0 and val < 1.0
    assert_equal(is_valid, true, "random range [0,1)")

// test: sample returns table of correct size
function sample_valid()
    t_samp = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    s = random.sample(t_samp, 3)
    is_valid = type(s) == "table" and table.size(s) == 3
    assert_equal(is_valid, true, "sample size correct")

// test: sample with k=0 returns empty table
function sample_k_zero()
    t_samp = [1, 2, 3]
    s = random.sample(t_samp, 0)
    is_valid = type(s) == "table" and table.size(s) == 0
    assert_equal(is_valid, true, "sample k=0 empty table")

// test: sample with k larger than table size returns none
function sample_k_too_large()
    t_samp = [1, 2, 3]
    res = random.sample(t_samp, 20)
    assert_equal(res, none, "sample k > size")

// test: seed ensures reproducible random sequence
function seed_reproducibility()
    random.seed(12345)
    r1 = random.float()
    random.seed(12345)
    r2 = random.float()
    assert_equal(r1, r2, "seed reproducibility")

// test: seed without arguments works
function seed_no_args()
    random.seed()
    r = random.float()
    assert_equal(type(r) == "number", true, "seed no args")

// test: shuffle preserves table size
function shuffle_preserves_size()
    t_shuf = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    original_size = table.size(t_shuf)
    random.shuffle(t_shuf)
    assert_equal(table.size(t_shuf), original_size, "shuffle preserves size")

// test: shuffle single element table works
function shuffle_single_element()
    t_single = ["val" = 1]
    random.shuffle(t_single)
    assert_equal(table.size(t_single), 1, "shuffle single element")

// test: shuffle empty table returns none
function shuffle_empty()
    t_empty = []
    res = random.shuffle(t_empty)
    assert_equal(res, none, "shuffle empty")

// test: triangular returns value within range
function triangular_range()
    tri = random.triangular(0, 10, 5)
    is_valid = type(tri) == "number" and tri >= 0 and tri <= 10
    assert_equal(is_valid, true, "triangular range")

// test: triangular with defaults uses [0,1] range
function triangular_defaults()
    tri = random.triangular()
    is_valid = type(tri) == "number" and tri >= 0 and tri <= 1
    assert_equal(is_valid, true, "triangular defaults")

// execute all random library test cases
betavariate_range()
betavariate_negative_alpha()
betavariate_negative_beta()
choice_valid()
choice_empty()
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
seed_reproducibility()
seed_no_args()
shuffle_preserves_size()
shuffle_single_element()
shuffle_empty()
triangular_range()
triangular_defaults()

// report completion of random library tests
os.output("[PASS] libraries/random.apex")