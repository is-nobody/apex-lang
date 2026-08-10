import os
import random

function guess_game()
    secret = random.integer(1, 10)
    attempts = 0
    os.output("I've picked a number between 1 and 10. Can you guess it?")
    
    for attempts < 999
        guess_str = os.input("Your guess: ")
        guess = number(guess_str)
        
        if guess == none
            os.output("Please enter a valid number.")
            continue
        
        attempts = attempts + 1
        
        if guess == secret
            os.output("Correct! You guessed it in {attempts} attempt(s)!")
            break
        elif guess < secret
            os.output("Higher!")
        else
            os.output("Lower!")

guess_game()