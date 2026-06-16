import random
import os
import table

function test_choices_valid()
    t_samp = (1, 2, 3, 4, 5)
    ch = random.choices(t_samp, 5)
    if type(ch) == "table" and table.size(ch) == 5
        os.output("PASS: choices() size correct")
    else
        os.output("FAIL: choices() size incorrect")

function test_choices_default_k()
    t_samp = (1, 2, 3)
    ch = random.choices(t_samp)
    if type(ch) == "table" and table.size(ch) == 1
        os.output("PASS: choices() default k=1")
    else
        os.output("FAIL: choices() default k failed")

test_choices_valid()
test_choices_default_k()