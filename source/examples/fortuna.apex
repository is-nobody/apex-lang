import os
import random
import table

function fortuna()
    answers = [
        "It is certain.", "Without a doubt.", "Yes definitely.",
        "Ask again later.", "Better not tell you now.",
        "Don't count on it.", "My sources say no.",
        "Outlook not so good.", "Very doubtful."
    ]

    os.output("🎱 Magic 8-Ball")
    question = os.input("Ask a yes/no question: ")

    idx = random.randint(1, table.size(answers))
    os.output(answers[idx])

fortuna()