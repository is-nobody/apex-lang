import os

function if_else_true()
    result = ""
    age = 25
    if age >= 18
        result = "adult"
    else
        result = "minor"
    os.output("age = 25, age >= 18 = true, result = {result}, expected adult")

function if_else_false()
    result = ""
    age = 16
    if age >= 18
        result = "adult"
    else
        result = "minor"
    os.output("age = 16, age >= 18 = false, result = {result}, expected minor")

function if_else_boundary_equal()
    result = ""
    age = 18
    if age >= 18
        result = "adult"
    else
        result = "minor"
    os.output("age = 18, age >= 18 = true, result = {result}, expected adult")

function if_else_negative_true()
    result = ""
    temp = -5
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    os.output("temp = -5, temp >= -10 = true, result = {result}, expected warm")

function if_else_negative_false()
    result = ""
    temp = -15
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    os.output("temp = -15, temp >= -10 = false, result = {result}, expected cold")

function if_else_negative_boundary_equal()
    result = ""
    temp = -10
    if temp >= -10
        result = "warm"
    else
        result = "cold"
    os.output("temp = -10, temp >= -10 = true, result = {result}, expected warm")

if_else_true()
if_else_false()
if_else_boundary_equal()
if_else_negative_true()
if_else_negative_false()
if_else_negative_boundary_equal()