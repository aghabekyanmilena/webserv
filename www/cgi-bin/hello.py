#!/usr/bin/env python3
import os
import sys

query = os.environ.get("QUERY_STRING", "")
method = os.environ.get("REQUEST_METHOD", "GET")
body = sys.stdin.read() if method == "POST" else ""

print("Content-Type: text/html")
print("Status: 200 OK")
print("")
print("<html><body>")
print("<h1>CGI hello</h1>")
print("<p>Method: %s</p>" % method)
print("<p>Query: %s</p>" % query)
if body:
    print("<p>Body length: %d</p>" % len(body))
print("</body></html>")
