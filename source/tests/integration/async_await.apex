// source/tests/integration/async_await.apex
// Implementation of Async/Await Integration Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

import os

// helper: compares two values and reports mismatch with context
function assert_equal(actual, expected, name)
    if actual != expected
        os.output("[ERROR] {name}: got {actual}, expected {expected}")

// helper: checks that a value is not none (future produced a real result)
function assert_not_none(value, name)
    if value == none
        os.output("[ERROR] {name}: unexpected none result")

// test: basic async function returns a future, await unwraps number
async function basic_add(a, b)
    return a + b

async function basic_async_call()
    fut = basic_add(2, 3)
    result = await fut
    assert_equal(result, 5, "basic async add")

// test: await returns value directly from inline call
async function inline_await()
    result = await basic_add(10, 20)
    assert_equal(result, 30, "inline await")

// test: async function with no explicit return yields none
async function no_return_func()
    x = 1

async function async_no_return()
    result = await no_return_func()
    assert_equal(result, none, "async no return")

// test: async returning string
async function greet(name)
    return "Hello, {name}!"

async function async_string_return()
    result = await greet("Apex")
    assert_equal(result, "Hello, Apex!", "async string return")

// test: async returning boolean
async function is_even(n)
    return n % 2 == 0

async function async_bool_return()
    assert_equal(await is_even(4), true, "async bool true")
    assert_equal(await is_even(7), false, "async bool false")

// test: async returning none explicitly
async function returns_none()
    return none

async function async_explicit_none()
    result = await returns_none()
    assert_equal(result, none, "async explicit none")

// test: await on non-future value is passthrough
async function await_passthrough_number()
    result = await 42
    assert_equal(result, 42, "await passthrough number")

async function await_passthrough_string()
    result = await "hello"
    assert_equal(result, "hello", "await passthrough string")

async function await_passthrough_bool()
    result = await true
    assert_equal(result, true, "await passthrough bool")

async function await_passthrough_none()
    result = await none
    assert_equal(result, none, "await passthrough none")

// test: same future awaited twice returns same value
async function await_twice()
    fut = basic_add(3, 4)
    first = await fut
    second = await fut
    assert_equal(first, 7, "await twice first")
    assert_equal(second, 7, "await twice second")

// test: body does not run until await
async function side_effect(counter_ref)
    counter_ref["ran"] = true
    return 1

async function deferred_body()
    flag = ["ran" = false]
    fut = side_effect(flag)
    assert_equal(flag["ran"], false, "body deferred until await")
    result = await fut
    assert_equal(result, 1, "async body result")
    assert_equal(flag["ran"], true, "body ran on await")

// test: nested await inside async body
async function twice(x)
    return x * 2

async function twice_then_add(x)
    doubled = await twice(x)
    return doubled + 1

async function nested_await()
    result = await twice_then_add(5)
    assert_equal(result, 11, "nested await (5*2+1)")

// test: deep nested await chain
async function inc(x)
    return x + 1

async function inc3(x)
    a = await inc(x)
    b = await inc(a)
    c = await inc(b)
    return c

async function deep_nested()
    result = await inc3(0)
    assert_equal(result, 3, "deep nested inc3(0)")

// test: recursive async fib
async function async_fib(n)
    if n < 2
        return n
    a = await async_fib(n - 1)
    b = await async_fib(n - 2)
    return a + b

async function recursive_async()
    assert_equal(await async_fib(0), 0, "fib(0)")
    assert_equal(await async_fib(1), 1, "fib(1)")
    assert_equal(await async_fib(5), 5, "fib(5)")
    assert_equal(await async_fib(10), 55, "fib(10)")

// test: async with multiple arguments
async function add3(a, b, c)
    return a + b + c

async function async_multi_args()
    result = await add3(1, 2, 3)
    assert_equal(result, 6, "async add3")

// test: async capturing arguments by value at call time
async function capture(x)
    return x

async function arg_captured_at_call()
    n = 10
    fut = capture(n)
    n = 99
    result = await fut
    assert_equal(result, 10, "arg captured at call time")

// test: multiple pending futures, resolved on demand
async function many_futures()
    f1 = basic_add(1, 1)
    f2 = basic_add(2, 2)
    f3 = basic_add(3, 3)
    assert_equal(await f1, 2, "many futures f1")
    assert_equal(await f2, 4, "many futures f2")
    assert_equal(await f3, 6, "many futures f3")

// test: await in expression context
async function await_in_expression()
    result = (await basic_add(1, 2)) * 10
    assert_equal(result, 30, "await in expression")

// test: await in condition
async function await_in_condition()
    if await basic_add(0, 1) == 1
        assert_equal(true, true, "await in if condition")
    else
        assert_equal(true, false, "await in if condition")

// test: await in for loop body
async function step(x)
    return x + 1

async function await_in_loop()
    total = 0
    for i = 1, 5
        total = await step(total)
    assert_equal(total, 5, "await in for loop body")

// test: async with table return value
async function make_table()
    return ["a" = 1, "b" = 2]

async function async_table_return()
    t = await make_table()
    assert_equal(t["a"], 1, "async table a")
    assert_equal(t["b"], 2, "async table b")

// test: async with table argument
async function sum_table(t)
    return t["x"] + t["y"]

async function async_table_arg()
    result = await sum_table(["x" = 5, "y" = 7])
    assert_equal(result, 12, "async table arg")

// test: async returning a future from another async call
async function wrap(x)
    return x

async function wrap_twice(x)
    inner = await wrap(x)
    return inner

async function async_return_through_await()
    result = await wrap_twice(42)
    assert_equal(result, 42, "async return through await")

// test: future used as table value
async function future_in_table()
    t = ["fut" = basic_add(2, 2)]
    result = await t["fut"]
    assert_equal(result, 4, "future stored in table")

// test: future passed as argument to async function
async function unwrap(f)
    return await f

async function future_as_arg()
    result = await unwrap(basic_add(7, 8))
    assert_equal(result, 15, "future passed to function")

// test: async function calling another async function without await keeps future
async function outer_keeps_future()
    fut = basic_add(3, 3)
    return fut

async function async_returning_future()
    inner = await outer_keeps_future()
    result = await inner
    assert_equal(result, 6, "async returning future unwraps twice")

// test: await inside ternary
async function pick(cond, a, b)
    result = a if cond == true else b
    return result

async function await_in_ternary()
    result = await pick(true, "yes", "no")
    assert_equal(result, "yes", "await in ternary true")
    result2 = await pick(false, "yes", "no")
    assert_equal(result2, "no", "await in ternary false")

// test: async function with early return
async function early(n)
    if n < 0
        return "negative"
    return "non-negative"

async function async_early_return()
    assert_equal(await early(-1), "negative", "async early return")
    assert_equal(await early(1), "non-negative", "async early return 2")

// test: multiple pending futures resolved on demand
async function interleaved_pending()
    a = basic_add(1, 1)
    b = basic_add(2, 2)
    c = basic_add(3, 3)
    assert_equal(await a, 2, "interleaved a")
    assert_equal(await b, 4, "interleaved b")
    assert_equal(await c, 6, "interleaved c")

// test: async with interpolation inside body
async function format(name, age)
    return "{name} is {age}"

async function async_interpolation()
    result = await format("Bob", 30)
    assert_equal(result, "Bob is 30", "async interpolation")

// test: async in nested function scope
async function async_nested_scope()
    async function inner_add(a, b)
        return a + b
    result = await inner_add(4, 5)
    assert_equal(result, 9, "async nested scope")

// test: many sequential awaits
async function sequential_awaits()
    total = 0
    total = total + await step(0)
    total = total + await step(0)
    total = total + await step(0)
    assert_equal(total, 3, "sequential awaits")

// test: await inside match case subject
async function classify(n)
    return n

async function await_in_match()
    result = ""
    x = await classify(2)
    match x
        case 1
            result = "one"
        case 2
            result = "two"
        case
            result = "other"
    assert_equal(result, "two", "await in match subject")

// test: future created inside loop, awaited after
async function futures_in_loop()
    futures = []
    for i = 1, 3
        futures[i] = basic_add(i, i)
    total = 0
    for i = 1, 3
        total = total + await futures[i]
    assert_equal(total, 12, "futures in loop (2+4+6)")

// test: nested async with multiple args and awaits
async function chain(a, b)
    x = await basic_add(a, b)
    y = await basic_add(x, a)
    z = await basic_add(y, b)
    return z

async function async_chain()
    result = await chain(1, 2)
    assert_equal(result, 6, "async chain (1+2)+(3+1)+(4+2)=6")

// test: async function with no parameters
async function constant_async()
    return 123

async function async_no_params()
    assert_equal(await constant_async(), 123, "async no params")

// test: async recursion depth
async function countdown(n)
    if n <= 0
        return 0
    return 1 + await countdown(n - 1)

async function async_countdown()
    assert_equal(await countdown(5), 5, "async countdown 5")
    assert_equal(await countdown(20), 20, "async countdown 20")

// execute all async/await test cases
await basic_async_call()
await inline_await()
await async_no_return()
await async_string_return()
await async_bool_return()
await async_explicit_none()
await await_passthrough_number()
await await_passthrough_string()
await await_passthrough_bool()
await await_passthrough_none()
await await_twice()
await deferred_body()
await nested_await()
await deep_nested()
await recursive_async()
await async_multi_args()
await arg_captured_at_call()
await many_futures()
await await_in_expression()
await await_in_condition()
await await_in_loop()
await async_table_return()
await async_table_arg()
await async_return_through_await()
await future_in_table()
await future_as_arg()
await async_returning_future()
await await_in_ternary()
await async_early_return()
await interleaved_pending()
await async_interpolation()
await async_nested_scope()
await sequential_awaits()
await await_in_match()
await futures_in_loop()
await async_chain()
await async_no_params()
await async_countdown()

// report completion of async/await integration tests
os.output("[PASS] integration/async_await.apex")