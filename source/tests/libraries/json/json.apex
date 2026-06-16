import json
import os

num = 42
str = "hello"
bool_val = true

json_num = json.encode(num)
os.output("Number: {json_num}")

json_str = json.encode(str)
os.output("String: {json_str}")

json_bool = json.encode(bool_val)
os.output("Bool: {json_bool}")

arr = ("apple", "banana", "cherry")
json_arr = json.encode(arr)
os.output("Array: {json_arr}")

obj = (name = "Apex", version = 1.0, is_cool = true)
json_obj = json.encode(obj)
os.output("Object: {json_obj}")

complex = (
    users = (
        (id = 1, name = "Alice"),
        (id = 2, name = "Bob")
    ),
    count = 2,
    active = true
)
json_complex = json.encode(complex)
os.output("Complex: {json_complex}")

raw_json = "\{\"status\": \"ok\", \"codes\": [200, 404], \"meta\": \{\"debug\": true\}\}"
data = json.decode(raw_json)

if type(data) == "table"
    os.output("Decoded status: {data.status}")
    os.output("First code: {string(data.codes.1)}")
    os.output("Debug mode: {string(data.meta.debug)}")
else
    os.output("Failed to decode JSON")

original = (key = "value", list = (1, 2, 3))
encoded = json.encode(original)
decoded = json.decode(encoded)

if decoded.key == "value" and decoded.list.1 == 1
    os.output("Round-trip successful!")
else
    os.output("Round-trip failed.")