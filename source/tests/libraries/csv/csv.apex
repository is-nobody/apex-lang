import os
import files
import csv
import table

data = (
    (id = 1, name = "Alice", age = 30, active = true),
    (id = 2, name = "Bob, Jr.", age = 25, active = false),
    (id = 3, name = "Charlie \"\"The Great\"\"", age = 40, active = true)
)

filename = "test_auto.csv"

result = csv.write(filename, data)

read_data = csv.read(filename)

if type(read_data) == "table"
    os.output("Read successful. Rows: {string(table.size(read_data))}")
    
    row1 = read_data.1
    if type(row1) == "table"
        os.output("Row 1 Name: {row1.name}")
        os.output("Row 1 Age Type: {type(row1.age)}")
        
        row2 = read_data.2
        if type(row2) == "table"
            os.output("Row 2 Name (with comma): {row2.name}")

        row3 = read_data.3
        if type(row3) == "table"
            os.output("Row 3 Name (with quotes): {row3.name}")
else
    os.output("Read failed or returned non-table value.")

if files.exists(filename) == true
    files.rmfile(filename)
else
    os.output("File not found for deletion.")