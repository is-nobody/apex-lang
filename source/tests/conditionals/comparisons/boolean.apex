import os

function boolean_equal_false()
    result = true == false
    os.output("true == false = {result}, expected false")

function boolean_equal_true()
    result = true == true
    os.output("true == true = {result}, expected true")

function boolean_not_equal_false()
    result = true != true
    os.output("true != true = {result}, expected false")

function boolean_not_equal_true()
    result = true != false
    os.output("true != false = {result}, expected true")

boolean_equal_false()
boolean_equal_true()
boolean_not_equal_false()
boolean_not_equal_true()