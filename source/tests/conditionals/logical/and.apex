import os

function and_false_false()
    result = false and false
    os.output("false and false = {result}, expected false")

function and_false_true()
    result = false and true
    os.output("false and true = {result}, expected false")

function and_true_false()
    result = true and false
    os.output("true and false = {result}, expected false")

function and_true_true()
    result = true and true
    os.output("true and true = {result}, expected true")

and_false_false()
and_false_true()
and_true_false()
and_true_true()