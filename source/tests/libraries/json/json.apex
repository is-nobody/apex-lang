import json
import os

num = 42
str = "hello"
bool_val = true

json_num = json.write(num)
os.output("Number: {json_num}")

json_str = json.write(str)
os.output("String: {json_str}")

json_bool = json.write(bool_val)
os.output("Bool: {json_bool}")

arr = ("apple", "banana", "cherry")
json_arr = json.write(arr)
os.output("Array: {json_arr}")

obj = (name = "Apex", version = 1.0, is_cool = true)
json_obj = json.write(obj)
os.output("Object: {json_obj}")

complex = (
    users = (
        (id = 1, name = "Alice"),
        (id = 2, name = "Bob")
    ),
    count = 2,
    active = true
)
json_complex = json.write(complex)
os.output("Complex: {json_complex}")

raw_json = "\{\"status\": \"ok\", \"codes\": [200, 404], \"meta\": \{\"debug\": true\}\}"
data = json.read(raw_json)

if type(data) == "table"
    os.output("readd status: {data.status}")
    os.output("First code: {string(data.codes.1)}")
    os.output("Debug mode: {string(data.meta.debug)}")
else
    os.output("Failed to read JSON")

original = (key = "value", list = (1, 2, 3))
writed = json.write(original)
readd = json.read(writed)

if readd.key == "value" and readd.list.1 == 1
    os.output("Round-trip successful!")
else
    os.output("Round-trip failed.")