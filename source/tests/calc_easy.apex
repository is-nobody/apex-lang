import os

function calculator()
    num1 = number(os.input("Enter first number: "))
    num2 = number(os.input("Enter second number: "))
    operation = os.input("Enter operation (+, -, *, /, %): ")

    if operation == "+"
        result = num1 + num2
    elif operation == "-"
        result = num1 - num2
    elif operation == "*"
        result = num1 * num2
    elif operation == "/"
        if num2 == 0
            return "Error: division by zero!"
        result = num1 / num2
    elif operation == "%"
        result = num1 % num2
    else
        return "Error: Invalid operation!"

    return result
    
calc_result = calculator()
os.output(calc_result)