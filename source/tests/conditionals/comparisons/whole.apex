import os

// ==
function whole_equal_false()
    result = 5 == 3
    os.output("5 == 3 = {result}, expected false")

function whole_equal_true()
    result = 5 == 5
    os.output("5 == 5 = {result}, expected true")

function whole_equal_negative_false()
    result = -5 == -3
    os.output("-5 == -3 = {result}, expected false")

function whole_equal_negative_true()
    result = -5 == -5
    os.output("-5 == -5 = {result}, expected true")

// !=
function whole_not_equal_false()
    result = 5 != 5
    os.output("5 != 5 = {result}, expected false")

function whole_not_equal_true()
    result = 5 != 3
    os.output("5 != 3 = {result}, expected true")

function whole_not_equal_negative_false()
    result = -5 != -5
    os.output("-5 != -5 = {result}, expected false")

function whole_not_equal_negative_true()
    result = -5 != -3
    os.output("-5 != -3 = {result}, expected true")

// <
function whole_less_than_equal_false()
    result = 5 < 5
    os.output("5 < 5 = {result}, expected false")

function whole_less_than_false()
    result = 10 < 5
    os.output("10 < 5 = {result}, expected false")

function whole_less_than_true()
    result = 5 < 10
    os.output("5 < 10 = {result}, expected true")

function whole_less_than_negative_equal_false()
    result = -5 < -5
    os.output("-5 < -5 = {result}, expected false")

function whole_less_than_negative_false()
    result = -5 < -10
    os.output("-5 < -10 = {result}, expected false")

function whole_less_than_negative_true()
    result = -10 < -5
    os.output("-10 < -5 = {result}, expected true")

// >
function whole_greater_than_equal_false()
    result = 5 > 5
    os.output("5 > 5 = {result}, expected false")

function whole_greater_than_false()
    result = 5 > 10
    os.output("5 > 10 = {result}, expected false")

function whole_greater_than_true()
    result = 10 > 5
    os.output("10 > 5 = {result}, expected true")

function whole_greater_than_negative_equal_false()
    result = -5 > -5
    os.output("-5 > -5 = {result}, expected false")

function whole_greater_than_negative_false()
    result = -10 > -5
    os.output("-10 > -5 = {result}, expected false")

function whole_greater_than_negative_true()
    result = -5 > -10
    os.output("-5 > -10 = {result}, expected true")

// <=
function whole_less_equal_equal_true()
    result = 5 <= 5
    os.output("5 <= 5 = {result}, expected true")

function whole_less_equal_false()
    result = 10 <= 5
    os.output("10 <= 5 = {result}, expected false")

function whole_less_equal_true()
    result = 5 <= 10
    os.output("5 <= 10 = {result}, expected true")

function whole_less_equal_negative_equal_true()
    result = -5 <= -5
    os.output("-5 <= -5 = {result}, expected true")

function whole_less_equal_negative_false()
    result = -5 <= -10
    os.output("-5 <= -10 = {result}, expected false")

function whole_less_equal_negative_true()
    result = -10 <= -5
    os.output("-10 <= -5 = {result}, expected true")

// >=
function whole_greater_equal_equal_true()
    result = 5 >= 5
    os.output("5 >= 5 = {result}, expected true")

function whole_greater_equal_false()
    result = 5 >= 10
    os.output("5 >= 10 = {result}, expected false")

function whole_greater_equal_true()
    result = 10 >= 5
    os.output("10 >= 5 = {result}, expected true")

function whole_greater_equal_negative_equal_true()
    result = -5 >= -5
    os.output("-5 >= -5 = {result}, expected true")

function whole_greater_equal_negative_false()
    result = -10 >= -5
    os.output("-10 >= -5 = {result}, expected false")

function whole_greater_equal_negative_true()
    result = -5 >= -10
    os.output("-5 >= -10 = {result}, expected true")

whole_equal_false()
whole_equal_true()
whole_equal_negative_false()
whole_equal_negative_true()
whole_not_equal_false()
whole_not_equal_true()
whole_not_equal_negative_false()
whole_not_equal_negative_true()
whole_less_than_equal_false()
whole_less_than_false()
whole_less_than_true()
whole_less_than_negative_equal_false()
whole_less_than_negative_false()
whole_less_than_negative_true()
whole_greater_than_equal_false()
whole_greater_than_false()
whole_greater_than_true()
whole_greater_than_negative_equal_false()
whole_greater_than_negative_false()
whole_greater_than_negative_true()
whole_less_equal_equal_true()
whole_less_equal_false()
whole_less_equal_true()
whole_less_equal_negative_equal_true()
whole_less_equal_negative_false()
whole_less_equal_negative_true()
whole_greater_equal_equal_true()
whole_greater_equal_false()
whole_greater_equal_true()
whole_greater_equal_negative_equal_true()
whole_greater_equal_negative_false()
whole_greater_equal_negative_true()