import os
import base
import string

complex_text = "Line 1\nLine 2\tTabbed\n\"Quoted\" & 'Ampersand'"

b32_str = base.b32write(complex_text)
os.output("Base32 Encoded: {b32_str}")

decoded_complex = base.b32read(b32_str)
if decoded_complex == complex_text
    os.output("Complex Text Round-trip: OK")
else
    os.output("Complex Text Round-trip: FAIL")

url_path = "/api/v1/search?q=hello world&lang=ru&special=@#$%"
encoded_url = base.url_write(url_path)
os.output("Encoded URL: {encoded_url}")

decoded_url = base.url_read(encoded_url)
if decoded_url == url_path
    os.output("Complex URL Round-trip: OK")
else
    os.output("Complex URL Round-trip: FAIL")

large_data = ""
i = 0
for i = 1, 100
    large_data = "{large_data}ApexLang_"

hex_large = base.b16write(large_data)
decoded_large = base.b16read(hex_large)

if decoded_large == large_data
    os.output("Large Data Hex Round-trip: OK")
else
    os.output("Large Data Hex Round-trip: FAIL")