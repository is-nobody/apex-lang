import os

function spinner()
    frames = ["|", "/", "-", "\\"]

    for i = 1, 20
        frame = frames[((i - 1) % 4) + 1]
        os.execute("clear")
        os.output("Working... {frame}")
        os.wait(0.2)

    os.output("Done!")

spinner()