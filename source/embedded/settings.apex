import os

function settings()
    user = "guest"
    platform = os.platform()
    language = "English"
    os.output("User: {user}")
    os.output("Platform: {platform}")
    os.output("Language: {language}")

settings()