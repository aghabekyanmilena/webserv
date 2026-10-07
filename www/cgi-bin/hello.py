#!/usr/bin/env python3
"""CGI demonstration: query arguments, POST body, and working directory."""
import html
import os
import sys
from urllib.parse import parse_qs

query_string = os.environ.get("QUERY_STRING", "")
query = parse_qs(query_string)
name = html.escape(query.get("name", ["WE"])[0])
body = sys.stdin.buffer.read()
message = html.escape(body.decode("utf-8", errors="replace"))
with open("message.txt", encoding="utf-8") as note:
    relative_file = html.escape(note.read().strip())
page = (
    "<!doctype html><html><body><h1>Hello, " + name + "!</h1>"
    "<p>Method: " + html.escape(os.environ.get("REQUEST_METHOD", "")) + "</p>"
    "<p>Query: " + html.escape(query_string) + "</p>"
    "<p>Body length (bytes): " + str(len(body)) + "</p>"
    "<p>Relative file: " + relative_file + "</p><pre>" + message + "</pre></body></html>"
).encode("utf-8")
sys.stdout.buffer.write(b"Content-Type: text/html; charset=utf-8\r\nStatus: 200 OK\r\n\r\n" + page)
sys.stdout.buffer.flush()
