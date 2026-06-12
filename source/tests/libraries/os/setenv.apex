import os

function setenv()
    result = os.setenv("APEX_TEST_VAR", "hello_world")
    os.output("set result: {result}, expected true")
    value = os.getenv("APEX_TEST_VAR")
    os.output("get value: {value}, expected hello_world")
    os.output("match: {value == \"hello_world\"}, expected true")

setenv()