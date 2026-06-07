import os

function input_basic()
    user_text = os.input("Your input: ")
    os.output("You typed: {user_text}")

function input_number_conversion()
    user_number = os.input("Number: ")
    converted = number(user_number)
    
    if converted == false
        os.output("'{user_number}' could not be converted to a number")
    else
        os.output("Converted value: {converted}")
        os.output("Double it: {converted * 2}")

input_basic()
input_number_conversion()