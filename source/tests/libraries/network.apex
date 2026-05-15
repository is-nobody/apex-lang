import os
import network

os.output("=== NETWORK TESTS ===")
os.output("")

// 1. url_encode
os.output("1. url_encode:")
e = network.url_encode("Hello World")
os.output("   {e}")
os.output("")

// 2. url_decode
os.output("2. url_decode:")
d = network.url_decode("Hello%20World")
os.output("   {d}")
os.output("")

// 3. parse_url
os.output("3. parse_url:")
p = network.parse_url("https://example.com:8080/path?q=1#top")
os.output("   scheme: {p.scheme}")
os.output("   host: {p.host}")
os.output("   port: {p.port}")
os.output("")

// 4. http_get
os.output("4. http_get:")
r = network.http_get("https://httpbin.org/get")
if r != none
    os.output("   OK")
else
    os.output("   FAILED")
os.output("")

// 5. http_post
os.output("5. http_post:")
body = (name = "Apex")
r2 = network.http_post("https://httpbin.org/post", body)
if r2 != none
    os.output("   OK")
else
    os.output("   FAILED")
os.output("")

// 6. dns_lookup
os.output("6. dns_lookup:")
ip = network.dns_lookup("google.com")
os.output("   google.com -> {ip}")
os.output("")

// 7. ip_is_valid
os.output("7. ip_is_valid:")
v1 = network.ip_is_valid("192.168.1.1")
v2 = network.ip_is_valid("999.999.999.999")
os.output("   192.168.1.1: {v1}")
os.output("   999.999.999.999: {v2}")
os.output("")

// 8. tcp_connect
os.output("8. tcp_connect:")
conn = network.tcp_connect("httpbin.org", 80)
if conn != none
    os.output("   Connected, id: {conn.id}")
    network.socket_close(conn.id)
else
    os.output("   FAILED")
os.output("")

// 9. tcp_listen
os.output("9. tcp_listen:")
srv = network.tcp_listen(18888)
if srv != none
    os.output("   Server listening, id: {srv.id}")
    network.server_close(srv.id)
else
    os.output("   FAILED")
os.output("")

os.output("=== DONE ===")