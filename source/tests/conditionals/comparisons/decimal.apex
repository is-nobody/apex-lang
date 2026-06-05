import os

// ==
function decimal_equal_false()
    result = 3.5 == 2.5
    os.output("3.5 == 2.5 = {result}, expected false")

function decimal_equal_true()
    result = 3.5 == 3.5
    os.output("3.5 == 3.5 = {result}, expected true")

function decimal_equal_negative_false()
    result = -5.5 == -3.5
    os.output("-5.5 == -3.5 = {result}, expected false")

function decimal_equal_negative_true()
    result = -5.5 == -5.5
    os.output("-5.5 == -5.5 = {result}, expected true")

// !=
function decimal_not_equal_false()
    result = 3.5 != 3.5
    os.output("3.5 != 3.5 = {result}, expected false")

function decimal_not_equal_true()
    result = 3.5 != 2.5
    os.output("3.5 != 2.5 = {result}, expected true")

function decimal_not_equal_negative_false()
    result = -5.5 != -5.5
    os.output("-5.5 != -5.5 = {result}, expected false")

function decimal_not_equal_negative_true()
    result = -5.5 != -3.5
    os.output("-5.5 != -3.5 = {result}, expected true")

// <
function decimal_less_than_equal_false()
    result = 3.5 < 3.5
    os.output("3.5 < 3.5 = {result}, expected false")

function decimal_less_than_false()
    result = 5.5 < 3.5
    os.output("5.5 < 3.5 = {result}, expected false")

function decimal_less_than_true()
    result = 3.5 < 5.5
    os.output("3.5 < 5.5 = {result}, expected true")

function decimal_less_than_negative_equal_false()
    result = -5.5 < -5.5
    os.output("-5.5 < -5.5 = {result}, expected false")

function decimal_less_than_negative_false()
    result = -5.5 < -10.5
    os.output("-5.5 < -10.5 = {result}, expected false")

function decimal_less_than_negative_true()
    result = -10.5 < -5.5
    os.output("-10.5 < -5.5 = {result}, expected true")

// >
function decimal_greater_than_equal_false()
    result = 5.5 > 5.5
    os.output("5.5 > 5.5 = {result}, expected false")

function decimal_greater_than_false()
    result = 3.5 > 5.5
    os.output("3.5 > 5.5 = {result}, expected false")

function decimal_greater_than_true()
    result = 5.5 > 3.5
    os.output("5.5 > 3.5 = {result}, expected true")

function decimal_greater_than_negative_equal_false()
    result = -5.5 > -5.5
    os.output("-5.5 > -5.5 = {result}, expected false")

function decimal_greater_than_negative_false()
    result = -10.5 > -5.5
    os.output("-10.5 > -5.5 = {result}, expected false")

function decimal_greater_than_negative_true()
    result = -5.5 > -10.5
    os.output("-5.5 > -10.5 = {result}, expected true")

// <=
function decimal_less_equal_equal_true()
    result = 3.5 <= 3.5
    os.output("3.5 <= 3.5 = {result}, expected true")

function decimal_less_equal_false()
    result = 5.5 <= 3.5
    os.output("5.5 <= 3.5 = {result}, expected false")

function decimal_less_equal_true()
    result = 3.5 <= 5.5
    os.output("3.5 <= 5.5 = {result}, expected true")

function decimal_less_equal_negative_equal_true()
    result = -5.5 <= -5.5
    os.output("-5.5 <= -5.5 = {result}, expected true")

function decimal_less_equal_negative_false()
    result = -5.5 <= -10.5
    os.output("-5.5 <= -10.5 = {result}, expected false")

function decimal_less_equal_negative_true()
    result = -10.5 <= -5.5
    os.output("-10.5 <= -5.5 = {result}, expected true")

// >=
function decimal_greater_equal_equal_true()
    result = 5.5 >= 5.5
    os.output("5.5 >= 5.5 = {result}, expected true")

function decimal_greater_equal_false()
    result = 3.5 >= 5.5
    os.output("3.5 >= 5.5 = {result}, expected false")

function decimal_greater_equal_true()
    result = 5.5 >= 3.5
    os.output("5.5 >= 3.5 = {result}, expected true")

function decimal_greater_equal_negative_equal_true()
    result = -5.5 >= -5.5
    os.output("-5.5 >= -5.5 = {result}, expected true")

function decimal_greater_equal_negative_false()
    result = -10.5 >= -5.5
    os.output("-10.5 >= -5.5 = {result}, expected false")

function decimal_greater_equal_negative_true()
    result = -5.5 >= -10.5
    os.output("-5.5 >= -10.5 = {result}, expected true")

decimal_equal_false()
decimal_equal_true()
decimal_equal_negative_false()
decimal_equal_negative_true()
decimal_not_equal_false()
decimal_not_equal_true()
decimal_not_equal_negative_false()
decimal_not_equal_negative_true()
decimal_less_than_equal_false()
decimal_less_than_false()
decimal_less_than_true()
decimal_less_than_negative_equal_false()
decimal_less_than_negative_false()
decimal_less_than_negative_true()
decimal_greater_than_equal_false()
decimal_greater_than_false()
decimal_greater_than_true()
decimal_greater_than_negative_equal_false()
decimal_greater_than_negative_false()
decimal_greater_than_negative_true()
decimal_less_equal_equal_true()
decimal_less_equal_false()
decimal_less_equal_true()
decimal_less_equal_negative_equal_true()
decimal_less_equal_negative_false()
decimal_less_equal_negative_true()
decimal_greater_equal_equal_true()
decimal_greater_equal_false()
decimal_greater_equal_true()
decimal_greater_equal_negative_equal_true()
decimal_greater_equal_negative_false()
decimal_greater_equal_negative_true()