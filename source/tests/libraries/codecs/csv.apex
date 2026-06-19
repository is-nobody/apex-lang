import os
import codecs
import table

data = (
    (id = 1, name = "Alice", age = 30, active = true),
    (id = 2, name = "Bob, Jr.", age = 25, active = false),
    (id = 3, name = "Charlie \"The Great\"", age = 40, active = true)
)

csv_string = codecs.csv_write(data)

os.output("Generated CSV:")
os.output(csv_string)

read_data = codecs.csv_read(csv_string)

if type(read_data) == "table"
    os.output("\nRead successful. Rows: {string(table.size(read_data))}")
    
    row1 = read_data.1
    if type(row1) == "table"
        os.output("Row 1 Name: {row1.name}")
        
        row3 = read_data.3
        if type(row3) == "table"
            os.output("Row 3 Name: {row3.name}")
else
    os.output("Read failed.")

if read_data.3.name == "Charlie \"The Great\""
    os.output("\nRound-trip successful!")
else
    os.output("\nRound-trip failed.")