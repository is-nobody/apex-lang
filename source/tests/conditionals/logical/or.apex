import os

function or_false_false()
    result = false or false
    os.output("false or false = {result}, expected false")

function or_false_true()
    result = false or true
    os.output("false or true = {result}, expected true")

function or_true_false()
    
    result = true or false
    os.output("true or false = {result}, expected true")

function or_true_true()
    result = true or true
    os.output("true or true = {result}, expected true")

or_false_false()
or_false_true()
or_true_false()
or_true_true()