import codecs
import os

edge_toml = "title = \"TOML Example\"\nis_cool = false\nrating = 9.5\nempty_val =\nquoted_num = \"42\""
edge_data = codecs.toml_read(edge_toml)

if type(edge_data) == "table"
    os.output("Title: {edge_data.title}")
    os.output("Is Cool: {string(edge_data.is_cool)}")
    os.output("Rating: {string(edge_data.rating)}")
    os.output("Empty val type: {type(edge_data.empty_val)}")
    os.output("Quoted Num: {edge_data.quoted_num}")
else
    os.output("Failed to read edge case TOML")