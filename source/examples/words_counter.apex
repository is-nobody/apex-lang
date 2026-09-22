import os
import regex
import table

function count_words(text)
    words = regex.find_all("\\S+", text)
    return table.size(words)

arg = os.args[1]
content = os.read(arg)

if content == none
    os.output("Cannot read {arg}")
else
    total = count_words(content)
    os.output("Words in {arg}: {total}")