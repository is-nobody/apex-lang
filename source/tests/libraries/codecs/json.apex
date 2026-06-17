import codecs
import os

num = 42
str = "hello"
bool_val = true

json_num = codecs.json_write(num)
os.output("Number: {json_num}")

json_str = codecs.json_write(str)
os.output("String: {json_str}")

json_bool = codecs.json_write(bool_val)
os.output("Bool: {json_bool}")

arr = ("apple", "banana", "cherry")
json_arr = codecs.json_write(arr)
os.output("Array: {json_arr}")

obj = (name = "Apex", version = 1.0, is_cool = true)
json_obj = codecs.json_write(obj)
os.output("Object: {json_obj}")

complex = (
    users = (
        (id = 1, name = "Alice"),
        (id = 2, name = "Bob")
    ),
    count = 2,
    active = true
)
json_complex = codecs.json_write(complex)
os.output("Complex: {json_complex}")

raw_json = "\{\"status\": \"ok\", \"codes\": [200, 404], \"meta\": \{\"debug\": true\}\}"
data = codecs.json_read(raw_json)

if type(data) == "table"
    os.output("readd status: {data.status}")
    os.output("First code: {string(data.codes.1)}")
    os.output("Debug mode: {string(data.meta.debug)}")
else
    os.output("Failed to read JSON")

original = (key = "value", list = (1, 2, 3))
writed = codecs.json_write(original)
readd = codecs.json_read(writed)

if readd.key == "value" and readd.list.1 == 1
    os.output("Round-trip successful!")
else
    os.output("Round-trip failed.")