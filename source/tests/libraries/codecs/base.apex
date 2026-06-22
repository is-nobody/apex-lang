import os
import codecs

text = "Hello, World! This is a test of Base64."
b64_str = codecs.base_write(text)
os.output("Base64 Encoded: {b64_str}")

decoded_b64 = codecs.base_read(b64_str)
if decoded_b64 == text
    os.output("Standard Base64 Round-trip: OK")
else
    os.output("Standard Base64 Round-trip: FAIL")

url_data = "data?param=value&other=123#fragment"
b64url_str = codecs.baseurl_write(url_data)
os.output("URL-Safe Base64 Encoded: {b64url_str}")

decoded_b64url = codecs.baseurl_read(b64url_str)
if decoded_b64url == url_data
    os.output("URL-Safe Base64 Round-trip: OK")
else
    os.output("URL-Safe Base64 Round-trip: FAIL")

binary_text = "\x00\x01\x02\xFF\xFE"
b64_bin = codecs.base_write(binary_text)
decoded_bin = codecs.base_read(b64_bin)
if decoded_bin == binary_text
    os.output("Binary Base64 Round-trip: OK")
else
    os.output("Binary Base64 Round-trip: FAIL")