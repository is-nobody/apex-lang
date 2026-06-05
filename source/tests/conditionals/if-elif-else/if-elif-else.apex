import os

function if_elif_else_first()
    result = ""
    score = 95
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 95, result = {result}, expected A")

function if_elif_else_middle()
    result = ""
    score = 85
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 85, result = {result}, expected B")

function if_elif_else_last()
    result = ""
    score = 75
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 75, result = {result}, expected C")

function if_elif_else_else()
    result = ""
    score = 65
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 65, result = {result}, expected F")

function if_elif_else_boundary_first()
    result = ""
    score = 90
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 90, result = {result}, expected A")

function if_elif_else_boundary_middle()
    result = ""
    score = 80
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 80, result = {result}, expected B")

function if_elif_else_boundary_last()
    result = ""
    score = 70
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = 70, result = {result}, expected C")

function if_elif_else_negative_all()
    result = ""
    score = -10
    if score >= 90
        result = "A"
    elif score >= 80
        result = "B"
    elif score >= 70
        result = "C"
    else
        result = "F"
    os.output("score = -10, result = {result}, expected F")

if_elif_else_first()
if_elif_else_middle()
if_elif_else_last()
if_elif_else_else()
if_elif_else_boundary_first()
if_elif_else_boundary_middle()
if_elif_else_boundary_last()
if_elif_else_negative_all()