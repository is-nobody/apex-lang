import os

function if_single_true()
    result = ""
    x = 10
    if x > 5
        result = "big"
    os.output("x = 10, x > 5 = true, result = {result}, expected big")

function if_single_false()
    result = ""
    x = 3
    if x > 5
        result = "big"
    os.output("x = 3, x > 5 = false, result = {result}, expected empty")

function if_single_negative_true()
    result = ""
    x = -3
    if x > -5
        result = "greater"
    os.output("x = -3, x > -5 = true, result = {result}, expected greater")

function if_single_negative_false()
    result = ""
    x = -10
    if x > -5
        result = "greater"
    os.output("x = -10, x > -5 = false, result = {result}, expected empty")

if_single_true()
if_single_false()
if_single_negative_true()
if_single_negative_false()
