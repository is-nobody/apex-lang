import os

function nested_if_below()
    result = ""
    x = 5
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("x = 5, result = {result}, expected below")

function nested_if_between()
    result = ""
    x = 15
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("x = 15, result = {result}, expected between")

function nested_if_above()
    result = ""
    x = 25
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("x = 25, result = {result}, expected above")

function nested_if_boundary_lower()
    result = ""
    x = 10
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("x = 10 (boundary >), result = {result}, expected below")

function nested_if_boundary_inner()
    result = ""
    x = 20
    if x > 10
        if x < 20
            result = "between"
        else
            result = "above"
    else
        result = "below"
    os.output("x = 20 (boundary <), result = {result}, expected above")

function nested_if_negative_below()
    result = ""
    x = -15
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    os.output("x = -15, result = {result}, expected negative_below")

function nested_if_negative_between()
    result = ""
    x = -5
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    os.output("x = -5, result = {result}, expected negative_between")

function nested_if_negative_above()
    result = ""
    x = 5
    if x > -10
        if x < 0
            result = "negative_between"
        else
            result = "negative_above"
    else
        result = "negative_below"
    os.output("x = 5, result = {result}, expected negative_above")

function nested_if_triple()
    result = ""
    x = 15
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 15, y = 25, result = {result}, expected nested")

function nested_if_triple_x_small()
    result = ""
    x = 5
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 5, y = 25, result = {result}, expected x_small")

function nested_if_triple_y_small()
    result = ""
    x = 15
    y = 15
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 15, y = 15, result = {result}, expected y_small")

function nested_if_triple_not_nested()
    result = ""
    x = 25
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 25, y = 25, result = {result}, expected not_nested")

function nested_if_triple_boundary_x()
    result = ""
    x = 10
    y = 25
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 10 (boundary), y = 25, result = {result}, expected x_small")

function nested_if_triple_boundary_y()
    result = ""
    x = 15
    y = 20
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 15, y = 20 (boundary), result = {result}, expected y_small")

function nested_if_triple_boundary_equal()
    result = ""
    x = 30
    y = 30
    if x > 10
        if y > 20
            if x < y
                result = "nested"
            else
                result = "not_nested"
        else
            result = "y_small"
    else
        result = "x_small"
    os.output("x = 30, y = 30 (equal), result = {result}, expected not_nested")

function nested_if_triple_negative()
    result = ""
    x = -5
    y = -15
    if x > -10
        if y > -20
            if x > y
                result = "neg_nested"
            else
                result = "neg_not_nested"
        else
            result = "neg_y_small"
    else
        result = "neg_x_small"
    os.output("x = -5, y = -15, result = {result}, expected neg_nested")


nested_if_below()
nested_if_between()
nested_if_above()
nested_if_boundary_lower()
nested_if_boundary_inner()
nested_if_negative_below()
nested_if_negative_between()
nested_if_negative_above()
nested_if_triple()
nested_if_triple_x_small()
nested_if_triple_y_small()
nested_if_triple_not_nested()
nested_if_triple_boundary_x()
nested_if_triple_boundary_y()
nested_if_triple_boundary_equal()
nested_if_triple_negative()